/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozofs.

  Rozofs is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozofs is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/vfs.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include "config.h"
#include "log.h"
#include "xmalloc.h"
#include "list.h"
#include "export.h"
#include "rozofs.h"
#include "volume.h"
#include "storageclt.h"

#define EHSIZE 2048

#define EBLOCKSKEY	"user.rozofs.export.blocks"
#define ETRASHUUID	"user.rozofs.export.trashid"
#define EFILESKEY	"user.rozofs.export.files"
#define EVERSIONKEY	"user.rozofs.export.version"
#define EATTRSTKEY	"user.rozofs.export.file.attrs"

static inline char *export_map(export_t * e, const char *vpath, char *path) {
    strcpy(path, e->root);
    strcat(path, vpath);
    return path;
}

static inline char *export_trash_map(export_t * e, fid_t fid, char *path) {
    char fid_str[37];
    uuid_unparse(fid, fid_str);
    strcpy(path, e->root);
    strcat(path, "/");
    strcat(path, e->trashname);
    strcat(path, "/");
    strcat(path, fid_str);
    return path;
}

static inline char *export_unmap(export_t * e, const char *path, char *vpath) {
    strcpy(vpath, path + strlen(e->root));
    return vpath;
}

static int export_check_root(const char *root) {
    int status = -1;
    struct stat st;

    DEBUG_FUNCTION;

    if (stat(root, &st) == -1) {
        goto out;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        goto out;
    }
    status = 0;
out:
    return status;
}

// Just check if VERSION is set.

static int export_check_setup(const char *root) {
    char version[20];

    DEBUG_FUNCTION;

    return getxattr(root, EVERSIONKEY, &version, 20) == -1 ? -1 : 0;
}

typedef struct mfentry {
    struct mfentry *parent;
    fid_t pfid;                 // Parent UUID
    char *path;                 // Absolute path on underlying fs
    char *name;                 // Name of file
    int fd;                     // File descriptor
    uint16_t cnt;               // Open counter
    mattr_t attrs;              // meta file attr
    list_t list;
} mfentry_t;

typedef struct rmfentry {
    fid_t fid;
    sid_t sids[ROZOFS_SAFE_MAX];
    list_t list;
} rmfentry_t;

static int mfentry_initialize(mfentry_t * mfe, mfentry_t * parent,
        const char *name, char *path) {
    int status = -1;
    DEBUG_FUNCTION;

    mfe->parent = parent;
    mfe->path = xstrdup(path);
    mfe->name = xstrdup(name);

    if (parent != NULL) {
        memcpy(mfe->pfid, parent->attrs.fid, sizeof (fid_t));
    } else {
        uuid_clear(mfe->pfid);
    }

    // Open the fd is not necessary now
    mfe->fd = -1;
    // The counter is initialized to zero
    mfe->cnt = 0;

    if (getxattr(path, EATTRSTKEY, &(mfe->attrs), sizeof (mattr_t)) == -1) {
        warning("mfentry_initialize failed: getxattr for file %s failed: %s",
                path, strerror(errno));
        goto error;
    }

    list_init(&mfe->list);
    status = 0;
    goto out;
error:
    if (mfe->path)
        free(mfe->path);
    if (mfe->name)
        free(mfe->name);
    mfe->path = NULL;
    mfe->name = NULL;
    mfe->parent = NULL;
out:
    return status;
}

static void mfentry_release(mfentry_t * mfe) {
    if (mfe) {
        if (mfe->fd != -1)
            close(mfe->fd);
        if (mfe->path)
            free(mfe->path);
        if (mfe->name)
            free(mfe->name);
    }
}

static int mfentry_persist(mfentry_t * mfe) {
    return setxattr(mfe->path, EATTRSTKEY, &(mfe->attrs), sizeof (mattr_t),
            XATTR_REPLACE) != 0 ? -1 : 0;
}

static uint32_t mfentry_hash_fid(void *key) {
    uint32_t hash = 0;
    uint8_t *c;

    for (c = key; c != key + 16; c++)
        hash = *c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

static uint32_t mfentry_hash_fid_name(void *key) {
    mfentry_t *mfe = (mfentry_t *) key;
    uint32_t hash = 0;
    uint8_t *c;
    char *d;

    for (c = mfe->pfid; c != mfe->pfid + 16; c++)
        hash = *c + (hash << 6) + (hash << 16) - hash;
    for (d = mfe->name; *d != '\0'; d++)
        hash = *d + (hash << 6) + (hash << 16) - hash;
    return hash;
}

static int mfentry_cmp_fid(void *k1, void *k2) {
    return memcmp(k1, k2, sizeof (fid_t));
}

static int mfentry_cmp_fid_name(void *k1, void *k2) {

    mfentry_t *sk1 = (mfentry_t *) k1;
    mfentry_t *sk2 = (mfentry_t *) k2;

    if ((uuid_compare(sk1->pfid, sk2->pfid) == 0) &&
            (strcmp(sk1->name, sk2->name) == 0)) {
        return 0;
    } else {
        return 1;
    }
}

static int export_load_rmfentry(export_t * e) {
    int status = -1;
    DIR *dd = NULL;
    struct dirent *dp;
    rmfentry_t *rmfe = NULL;
    char trash_path[PATH_MAX + FILENAME_MAX + 1];

    DEBUG_FUNCTION;

    strcpy(trash_path, e->root);
    strcat(trash_path, "/");
    strcat(trash_path, e->trashname);

    if ((dd = opendir(trash_path)) == NULL) {
        severe("export_load_rmfentry failed: opendir failed: %s",
                strerror(errno));
        goto out;
    }

    while ((dp = readdir(dd)) != NULL) {

        if ((strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0)) {
            continue;
        }

        char rm_path[PATH_MAX + FILENAME_MAX + 1];
        uuid_t rmfid;
        mattr_t attrs;
        uuid_parse(dp->d_name, rmfid);

        if (getxattr
                (export_trash_map(e, rmfid, rm_path), EATTRSTKEY, &attrs,
                sizeof (mattr_t)) == -1) {
            severe
                    ("export_load_rmfentry failed: getxattr for file %s failed: %s",
                    export_trash_map(e, rmfid, rm_path), strerror(errno));
        }

        rmfe = xmalloc(sizeof (rmfentry_t));
        memcpy(rmfe->fid, attrs.fid, sizeof (fid_t));
        memcpy(rmfe->sids, attrs.sids, sizeof (sid_t) * ROZOFS_SAFE_MAX);

        list_init(&rmfe->list);

                if ((errno = pthread_rwlock_wrlock(&e->rm_lock)) != 0)
                    goto out;

        list_push_front(&e->rmfiles, &rmfe->list);

                if ((errno = pthread_rwlock_unlock(&e->rm_lock)) != 0)
                    goto out;
    }

    status = 0;
out:
    if (dd != NULL) {
        closedir(dd);
    }
    return status;
}

static void export_put_mfentry(export_t * e, mfentry_t * mfe) {

    DEBUG_FUNCTION;

    htable_put(&e->hfids, mfe->attrs.fid, mfe);
    htable_put(&e->h_pfids, mfe, mfe);

    list_push_front(&e->mfiles, &mfe->list);
}

static void export_del_mfentry(export_t * e, mfentry_t * mfe) {

    DEBUG_FUNCTION;

    htable_del(&e->hfids, mfe->attrs.fid);
    htable_del(&e->h_pfids, mfe);

    list_remove(&mfe->list);
}

static inline int export_update_files(export_t * e, int32_t n) {
    int status = -1;
    uint64_t files;

    if (getxattr(e->root, EFILESKEY, &files, sizeof (uint64_t)) == -1) {
        warning("export_update_files failed: getxattr for file %s failed: %s",
                e->root, strerror(errno));
        goto out;
    }

    files += n;

    if (setxattr(e->root, EFILESKEY, &files, sizeof (uint64_t), XATTR_REPLACE)
            == -1) {
        warning("export_update_files failed: setxattr for file %s failed: %s",
                e->root, strerror(errno));
        goto out;
    }
    status = 0;
out:

    return status;
}

static inline int export_update_blocks(export_t * e, int32_t n) {
    int status = -1;
    uint64_t blocks;
    DEBUG_FUNCTION;

    if (getxattr(e->root, EBLOCKSKEY, &blocks, sizeof (uint64_t)) !=
            sizeof (uint64_t)) {
        warning
                ("export_update_blocks failed: getxattr for file %s failed: %s",
                e->root, strerror(errno));
        goto out;
    }

    blocks += n;

    if (setxattr
            (e->root, EBLOCKSKEY, &blocks, sizeof (uint64_t),
            XATTR_REPLACE) != 0) {
        warning
                ("export_update_blocks failed: setxattr for file %s failed: %s",
                e->root, strerror(errno));
        goto out;
    }
    status = 0;
out:

    return status;
}

int export_create(const char *root) {
    int status = -1;
    const char *version = VERSION;
    char path[PATH_MAX];
    mattr_t attrs;
    uint64_t zero = 0;
    char trash_path[PATH_MAX + FILENAME_MAX + 1];
    uuid_t trash_uuid;
    char trash_str[37];
    DEBUG_FUNCTION;

    if (!realpath(root, path))
        goto out;
    if (export_check_root(path) != 0)
        goto out;
    if (setxattr(path, EBLOCKSKEY, &zero, sizeof (zero), XATTR_CREATE) != 0)
        goto out;
    if (setxattr(path, EFILESKEY, &zero, sizeof (zero), XATTR_CREATE) != 0)
        goto out;
    if (setxattr
            (path, EVERSIONKEY, &version, sizeof (char) * strlen(version) + 1,
            XATTR_CREATE) != 0)
        goto out;
    uuid_generate(attrs.fid);
    attrs.cid = 0;
    memset(attrs.sids, 0, ROZOFS_SAFE_MAX * sizeof (sid_t));
    attrs.mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
    attrs.nlink = 2;
    if ((attrs.ctime = attrs.atime = attrs.mtime = time(NULL)) == -1)
        goto out;
    attrs.size = ROZOFS_DIR_SIZE;
    if (setxattr(path, EATTRSTKEY, &attrs, sizeof (mattr_t), XATTR_CREATE)
            != 0)
        goto out;

    uuid_generate(trash_uuid);
    uuid_unparse(trash_uuid, trash_str);
    strcpy(trash_path, root);
    strcat(trash_path, "/");
    strcat(trash_path, trash_str);

    if (mkdir(trash_path, S_IRWXU) != 0)
        goto out;

    if (setxattr(path, ETRASHUUID, trash_uuid, sizeof (uuid_t), XATTR_CREATE)
            != 0)
        goto out;

    status = 0;
out:

    return status;
}

int export_initialize(export_t * e, uint32_t eid, const char *root) {
    int status = -1;
    mfentry_t *mfe;
    uuid_t trash_uuid;
    char trash_str[37];
    DEBUG_FUNCTION;

    if (!realpath(root, e->root))
        goto out;
    if (export_check_root(e->root) != 0)
        goto out;
    if (export_check_setup(e->root) != 0) {
        if (export_create(root) != 0) {
            goto out;
        }
    }

    e->eid = eid;
    list_init(&e->mfiles);
    list_init(&e->rmfiles);

        if ((errno = pthread_rwlock_init(&e->rm_lock, NULL)) != 0) {
            status = -1;
            goto out;
        }

    htable_initialize(&e->hfids, EHSIZE, mfentry_hash_fid, mfentry_cmp_fid);
    htable_initialize(&e->h_pfids, EHSIZE, mfentry_hash_fid_name,
            mfentry_cmp_fid_name);

    // Register the root
    mfe = xmalloc(sizeof (mfentry_t));
    if (mfentry_initialize(mfe, 0, e->root, e->root) != 0)
        goto out;

    export_put_mfentry(e, mfe);
    memcpy(e->rfid, mfe->attrs.fid, sizeof (fid_t));

    if (getxattr(root, ETRASHUUID, &(trash_uuid), sizeof (uuid_t)) == -1) {
        severe("export_initialize failed: getxattr for file %s failed: %s",
                root, strerror(errno));
        goto out;
    }

    uuid_unparse(trash_uuid, trash_str);
    strcpy(e->trashname, trash_str);

    if (export_load_rmfentry(e) != 0) {
        severe("export_initialize failed: export_load_rmfentry failed: %s",
                strerror(errno));
        goto out;
    }

    status = 0;
out:

    return status;
}

void export_release(export_t * e) {

    list_t *p, *q;
    DEBUG_FUNCTION;

    list_for_each_forward_safe(p, q, &e->mfiles) {

        mfentry_t *mfe = list_entry(p, mfentry_t, list);
        export_del_mfentry(e, mfe);
        mfentry_release(mfe);
        free(mfe);
    }

    list_for_each_forward_safe(p, q, &e->rmfiles) {

        rmfentry_t *rmfe = list_entry(p, rmfentry_t, list);
        list_remove(&rmfe->list);
        free(rmfe);
    }

        pthread_rwlock_destroy(&e->rm_lock);

    htable_release(&e->hfids);
    htable_release(&e->h_pfids);
}

int export_stat(export_t * e, estat_t * st) {
    int status = -1;
    struct statfs stfs;
    volume_stat_t vstat;
    DEBUG_FUNCTION;

    st->bsize = ROZOFS_BSIZE;
    if (statfs(e->root, &stfs) != 0)
        goto out;
    st->namemax = stfs.f_namelen;
    st->ffree = stfs.f_ffree;
    if (getxattr(e->root, EBLOCKSKEY, &(st->blocks), sizeof (uint64_t)) == -1)
        goto out;
    volume_stat(&vstat);
    st->bfree = vstat.bfree;
    // blocks store in EBLOCKSKEY is the number of currently stored blocks
    // blocks in estat_t is the total number of blocks (see struct statvfs)
    // rozofs does not have a constant total number of blocks
    // it depends on usage made of storage (through other services)
    st->blocks += st->bfree;
    if (getxattr(e->root, EFILESKEY, &(st->files), sizeof (uint64_t)) == -1)
        goto out;

    status = 0;
out:

    return status;
}

int export_lookup(export_t * e, fid_t parent, const char *name,
        mattr_t * attrs) {
    int status = -1;
    char path[PATH_MAX + FILENAME_MAX + 1];
    mfentry_t *pmfe = 0;
    mfentry_t *mfe = 0;
    mfentry_t *mfkey = 0;
    DIR *dp = 0;
    struct dirent *de = 0;
    DEBUG_FUNCTION;

    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }
    // manage "." and ".."
    if (strcmp(name, ".") == 0) {
        memcpy(attrs, &pmfe->attrs, sizeof (mattr_t));
        status = 0;
        goto out;
    }
    // what if root ?
    if (strcmp(name, "..") == 0) {
        if (uuid_compare(parent, e->rfid) == 0) // we're looking for root parent
            memcpy(attrs, &pmfe->attrs, sizeof (mattr_t));
        else
            memcpy(attrs, &pmfe->parent->attrs, sizeof (mattr_t));
        status = 0;
        goto out;
    }

    if (strcmp(name, e->trashname) == 0) {
        errno = ENOENT;
        status = -1;
        goto out;
    }

    strcpy(path, pmfe->path);
    strcat(path, "/");
    strcat(path, name);

    mfkey = xmalloc(sizeof (mfentry_t));
    memcpy(mfkey->pfid, pmfe->attrs.fid, sizeof (fid_t));
    mfkey->name = xstrdup(name);

    // Check if already cached
    if (!(mfe = htable_get(&e->h_pfids, mfkey))) {
        // if not find and cache it.

        if (!(dp = opendir(pmfe->path)))
            goto out;
        while ((de = readdir(dp))) {
            if (strcmp(de->d_name, name) == 0) {
                mfe = xmalloc(sizeof (mfentry_t));
                if ((mfentry_initialize(mfe, pmfe, name, path)) != 0) {
                    free(mfe);
                    goto out;
                }
                export_put_mfentry(e, mfe);
                break;
            }
        }
    }
    if (mfe) {
        char to[PATH_MAX + NAME_MAX + 1];
        // Need to verify if the path is good (see rename)
        // Put the new path for the file
        strcpy(to, pmfe->path);
        strcat(to, "/");
        strcat(to, name);

        if (strcmp(mfe->path, to) != 0) {
            free(mfe->path);
            mfe->path = xstrdup(to);
        }

        memcpy(attrs, &mfe->attrs, sizeof (mattr_t));
        status = 0;
    } else {
        errno = ENOENT;
        status = -1;
    }
out:
    if (mfkey) {
        if (mfkey->name)
            free(mfkey->name);
        free(mfkey);
    }
    if (dp)
        closedir(dp);

    return status;
}

int export_getattr(export_t * e, fid_t fid, mattr_t * attrs) {
    int status = -1;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }
    memcpy(attrs, &mfe->attrs, sizeof (mattr_t));
    status = 0;
out:

    return status;
}

int export_setattr(export_t * e, fid_t fid, mattr_t * attrs) {
    int status = -1;
    mfentry_t *mfe = 0;
    dist_t empty = 0;
    int fd = -1;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }
    // XXX IS IT POSSIBLE WITH A DIRECTORY?
    if (mfe->attrs.size != attrs->size) {

        if (attrs->size >= 0x20000000000LL) {
            errno = EFBIG;
            goto out;
        }

        uint64_t nrb_new = ((attrs->size + ROZOFS_BSIZE - 1) / ROZOFS_BSIZE);
        uint64_t nrb_old =
                ((mfe->attrs.size + ROZOFS_BSIZE - 1) / ROZOFS_BSIZE);

        // Open the file descriptor
        if ((fd = open(mfe->path, O_RDWR)) < 0) {
            severe("export_setattr failed: open for file %s failed: %s",
                    mfe->path, strerror(errno));
               goto out;
        }
        if (mfe->attrs.size > attrs->size) {
            if (ftruncate(fd, nrb_new * sizeof (dist_t)) != 0)
                goto out;
        } else {
            off_t count = 0;
            for (count = nrb_old; count < nrb_new; count++) {
                if (pwrite
                        (fd, &empty, sizeof (dist_t),
                        count * sizeof (dist_t)) != sizeof (dist_t)) {
                    severe("export_setattr: pwrite failed : %s",
                            strerror(errno));
                    goto out;
                }
            }
        }
        if (export_update_blocks(e, ((int32_t) nrb_new - (int32_t) nrb_old))
                != 0)
            goto out;

        mfe->attrs.size = attrs->size;
    }

    mfe->attrs.mode = attrs->mode;
    mfe->attrs.nlink = attrs->nlink;
    mfe->attrs.ctime = time(NULL);
    // XXX if client time != exportd time, there may be a problem.
    mfe->attrs.atime = attrs->atime;
    mfe->attrs.mtime = attrs->mtime;

    if (mfentry_persist(mfe) != 0)
        goto out;
    status = 0;

out:
    if (fd != -1)
        close(fd);

    return status;
}

int export_readlink(export_t * e, uuid_t fid, char link[PATH_MAX]) {
    int status = -1;
    ssize_t len;
    mfentry_t *mfe;
    char rlink[PATH_MAX];
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if ((len = readlink(mfe->path, rlink, PATH_MAX)) == -1)
        goto out;

    rlink[len] = '\0';
    export_unmap(e, rlink, link);
    status = 0;
out:

    return status;
}

int export_mknod(export_t * e, uuid_t parent, const char *name, mode_t mode,
        mattr_t * attrs) {
    int status = -1;
    char path[PATH_MAX + FILENAME_MAX + 1];
    mfentry_t *pmfe = 0;
    mfentry_t *mfe = 0;
    int xerrno = errno;
    DEBUG_FUNCTION;

    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }

    strcpy(path, pmfe->path);
    strcat(path, "/");
    strcat(path, name);
    // XXX we could use an other mode
    if (mknod(path, mode, 0) != 0)
        goto out;

    uuid_generate(attrs->fid);
    if (volume_distribute(&attrs->cid, attrs->sids) != 0)
        goto error;

    attrs->mode = mode;
    attrs->nlink = 1;

    if ((attrs->ctime = attrs->atime = attrs->mtime = time(NULL)) == -1)
        goto error;

    attrs->size = 0;
    if (setxattr(path, EATTRSTKEY, attrs, sizeof (mattr_t), XATTR_CREATE) !=
            0)
        goto error;

    mfe = xmalloc(sizeof (mfentry_t));

    if (mfentry_initialize(mfe, pmfe, name, path) != 0)
        goto error;

    pmfe->attrs.mtime = pmfe->attrs.ctime = time(NULL);

    if (mfentry_persist(pmfe) != 0)
        goto error;

    export_put_mfentry(e, mfe);

    if (export_update_files(e, 1) != 0)
        goto error;

    status = 0;
    goto out;
error:
    xerrno = errno;
    if (mfe) {
        mfentry_release(mfe);
        free(mfe);
    }
    if (xerrno != EEXIST) {
        unlink(path);
    }
    errno = xerrno;
out:

    return status;
}

int export_mkdir(export_t * e, uuid_t parent, const char *name, mode_t mode,
        mattr_t * attrs) {
    int status = -1;
    char path[PATH_MAX + FILENAME_MAX + 1];
    mfentry_t *pmfe = 0;
    mfentry_t *mfe = 0;
    int xerrno;
    DEBUG_FUNCTION;

    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }
    strcpy(path, pmfe->path);
    strcat(path, "/");
    strcat(path, name);

    if (mkdir(path, mode) != 0)
        goto error;

    uuid_generate(attrs->fid);
    attrs->cid = 0;
    memset(attrs->sids, 0, ROZOFS_SAFE_MAX * sizeof (uint16_t));
    attrs->mode = mode;
    attrs->nlink = 2;

    if ((attrs->ctime = attrs->atime = attrs->mtime = time(NULL)) == -1)
        goto error;

    attrs->size = ROZOFS_BSIZE;

    if (setxattr(path, EATTRSTKEY, attrs, sizeof (mattr_t), XATTR_CREATE) !=
            0)
        goto error;

    mfe = xmalloc(sizeof (mfentry_t));

    if (mfentry_initialize(mfe, pmfe, name, path) != 0)
        goto error;

    export_put_mfentry(e, mfe);

    pmfe->attrs.nlink++;
    pmfe->attrs.mtime = pmfe->attrs.ctime = time(NULL);

    if (mfentry_persist(pmfe) != 0) {
        pmfe->attrs.nlink--;
        goto error;
    }

    if (export_update_files(e, 1) != 0)
        goto error;

    status = 0;
    goto out;
error:
    xerrno = errno;
    if (mfe) {
        mfentry_release(mfe);
        free(mfe);
    }
    if (xerrno != EEXIST) {
        rmdir(path);
    }
    errno = xerrno;
out:

    return status;
}

int export_unlink(export_t * e, uuid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    rmfentry_t *rmfe = 0;
    char path[PATH_MAX + NAME_MAX + 1];
    char rm_path[PATH_MAX + NAME_MAX + 1];
    uint64_t size = 0;
    mode_t mode;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    strcpy(path, mfe->path);
    size = mfe->attrs.size;
    mode = mfe->attrs.mode;

    // Put the new path for the remove file
    // If the size is equal to 0 ? not move hum I'm not sure beacause setxattr ?
    if (rename(path, export_trash_map(e, fid, rm_path)) == -1) {
        severe("export_unlink failed: rename(%s,%s) failed: %s", path,
                rm_path, strerror(errno));
        goto out;
    }

    rmfe = xmalloc(sizeof (rmfentry_t));
    memcpy(rmfe->fid, mfe->attrs.fid, sizeof (fid_t));
    memcpy(rmfe->sids, mfe->attrs.sids, sizeof (sid_t) * ROZOFS_SAFE_MAX);

    list_init(&rmfe->list);

        if ((errno = pthread_rwlock_wrlock(&e->rm_lock)) != 0)
        goto out;               // XXX PROBLEM: THE NODE IS RENAMED

    list_push_front(&e->rmfiles, &rmfe->list);

        if ((errno = pthread_rwlock_unlock(&e->rm_lock)) != 0)
        goto out;               // XXX PROBLEM: THE NODE IS RENAMED

    // Update times of parent
    if (mfe->parent != NULL) {
        mfe->parent->attrs.mtime = mfe->parent->attrs.ctime = time(NULL);
        if (mfentry_persist(mfe->parent) != 0) {
            goto out;           // XXX PROBLEM: THE NODE IS RENAMED
        }
    }

    export_del_mfentry(e, mfe);
    mfentry_release(mfe);
    free(mfe);

    if (export_update_files(e, -1) != 0)
        goto out;

    if (!S_ISLNK(mode))
        if (export_update_blocks
                (e, -(((int64_t) size + ROZOFS_BSIZE - 1) / ROZOFS_BSIZE)) != 0)
            goto out;

    status = 0;
out:

    return status;
}

int export_rm_bins(export_t * e) {
    int status = -1;
    int cnt = 0;
    list_t *p, *q;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_trywrlock(&e->rm_lock)) != 0)
        goto out;

    list_for_each_forward_safe(p, q, &e->rmfiles) {

        rmfentry_t *entry = list_entry(p, rmfentry_t, list);
        sid_t *it = entry->sids;
        cnt = 0;

        while (it != entry->sids + rozofs_safe) {

            if (*it != 0) {
                char host[ROZOFS_HOSTNAME_MAX];
                storageclt_t sclt;

                lookup_volume_storage(*it, host);

                if (storageclt_initialize(&sclt, host, *it) != 0) {
                    warning("failed to join: %s,  %s", host, strerror(errno));

                } else {
                    if (storageclt_remove(&sclt, entry->fid) != 0) {
                        warning("failed to remove: %s", host);
                    } else {
                        *it = 0;
                        cnt++;
                    }
                }
                storageclt_release(&sclt);
            } else {
                cnt++;
            }
            it++;
        }

        if (cnt == rozofs_safe) {

            char path[PATH_MAX + NAME_MAX + 1];

            if (unlink(export_trash_map(e, entry->fid, path)) == -1) {
                severe("export_rm_bins failed: unlink file %s failed: %s",
                        path, strerror(errno));
                goto out;
            }

            list_remove(&entry->list);
            free(entry);
        }
    }

        if ((errno = pthread_rwlock_unlock(&e->rm_lock)) != 0)
            goto out;

    status = 0;
out:
    return status;
}

int export_rmdir(export_t * e, uuid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (rmdir(mfe->path) == -1)
        goto out;

    if (export_update_files(e, -1) != 0)
        goto out; // XXX PROBLEM: THE DIRECTORY IS REMOVED

    // Update the nlink and times of parent
    if (mfe->parent != NULL) {
        mfe->parent->attrs.nlink--;
        mfe->parent->attrs.mtime = mfe->parent->attrs.ctime = time(NULL);
        if (mfentry_persist(mfe->parent) != 0) {
            mfe->parent->attrs.nlink++;
            goto out;           // XXX PROBLEM: THE DIRECTORY IS REMOVED
        }
    }

    export_del_mfentry(e, mfe);
    mfentry_release(mfe);
    free(mfe);

    status = 0;
out:
    return status;
}

// XXX PROBLEM: IT'S NOT POSSIBLE TO SET XATTR TO A SYMLINK

int export_symlink(export_t * e, const char *link_name, uuid_t parent,
        const char *name, mattr_t * attrs) {
    int status = -1;
    char path[PATH_MAX + FILENAME_MAX + 1];
    mfentry_t *pmfe = 0;
    mfentry_t *mfe = 0;
    int xerrno = errno;
    DEBUG_FUNCTION;

    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }

    strcpy(path, pmfe->path);
    strcat(path, "/");
    strcat(path, name);

    if (symlink(link_name, path) != 0) {
        severe("export_symlink failed: symlink(%s,%s) failed: %s", link_name,
                path, strerror(errno));
        goto out;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        goto out;
    }
    uuid_generate(attrs->fid);
    attrs->cid = 0;
    memset(attrs->sids, 0, ROZOFS_SAFE_MAX * sizeof (uint16_t));
    attrs->mode = st.st_mode;
    attrs->nlink = st.st_nlink;

    if ((attrs->ctime = attrs->atime = attrs->mtime = time(NULL)) == -1)
        goto error;

    attrs->size = st.st_size;

    if (setxattr(path, EATTRSTKEY, attrs, sizeof (mattr_t), XATTR_CREATE) !=
            0) {
        severe("export_symlink failed: lsetxattr(%s) failed: %s", path,
                strerror(errno));
        goto error;
    }

    mfe = xmalloc(sizeof (mfentry_t));

    if (mfentry_initialize(mfe, pmfe, name, path) != 0)
        goto error;

    if (export_update_files(e, 1) != 0)
        goto error;

    pmfe->attrs.nlink++;
    if (mfentry_persist(pmfe) != 0) {
        pmfe->attrs.nlink--;
        goto error;
    }

    export_put_mfentry(e, mfe);

    status = 0;
    goto out;
error:
    xerrno = errno;
    if (mfe) {
        mfentry_release(mfe);
        free(mfe);
    }
    if (xerrno != EEXIST) {
        unlink(path);
    }
out:

    return status;
}

int export_rename(export_t * e, uuid_t from, uuid_t parent, const char *name) {
    int status = -1;
    mfentry_t *fmfe = 0;
    mfentry_t *pmfe = 0;
    mfentry_t *ofmfe = 0;
    mfentry_t *to_mfkey = 0;
    char to[PATH_MAX + NAME_MAX + 1];
    uint64_t size = 0;
    mode_t mode;
    DEBUG_FUNCTION;

    // Get the mfe for the file
    if (!(fmfe = htable_get(&e->hfids, from))) {
        errno = ESTALE;
        goto out;
    }
    // Get the mfe for the new parent
    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }
    // Put the new path for the file
    strcpy(to, pmfe->path);
    strcat(to, "/");
    strcat(to, name);

    if (rename(fmfe->path, to) == -1)
        goto out;

    // Prepare the NEW key
    to_mfkey = xmalloc(sizeof (mfentry_t));
    memcpy(to_mfkey->pfid, pmfe->attrs.fid, sizeof (fid_t));
    to_mfkey->name = xstrdup(name);

    // If the target file or directory already existed
    if ((ofmfe = htable_get(&e->h_pfids, to_mfkey))) {

        if (S_ISDIR(ofmfe->attrs.mode)) {
            // Update the nlink of new parent
            pmfe->attrs.nlink--;
            if (mfentry_persist(pmfe) != 0) {
                pmfe->attrs.nlink++;
                goto out;
            }
        }
        // Delete the old newpath
        size = ofmfe->attrs.size;
        mode = ofmfe->attrs.mode;
        export_del_mfentry(e, ofmfe);
        mfentry_release(ofmfe);
        free(ofmfe);

        if (export_update_files(e, -1) != 0)
            goto out;

        if (!S_ISLNK(mode) && !S_ISDIR(mode))
            if (export_update_blocks
                    (e,
                    -(((int64_t) size + ROZOFS_BSIZE - 1) / ROZOFS_BSIZE)) != 0)
                goto out;
    }

    if (S_ISDIR(fmfe->attrs.mode)) {
        // Update the nlink of new parent
        pmfe->attrs.nlink++;
        if (mfentry_persist(pmfe) != 0) {
            pmfe->attrs.nlink--;
            goto out;
        }
        // Update the nlink of old parent
        fmfe->parent->attrs.nlink--;
        if (mfentry_persist(fmfe->parent) != 0) {
            fmfe->parent->attrs.nlink++;
            goto out;
        }
    }

    htable_del(&e->h_pfids, fmfe);

    // Change the path
    free(fmfe->path);
    fmfe->path = xstrdup(to);
    free(fmfe->name);
    fmfe->name = xstrdup(name);
    fmfe->attrs.ctime = time(NULL);
    // Put the new parent
    fmfe->parent = pmfe;
    memcpy(fmfe->pfid, pmfe->attrs.fid, sizeof (fid_t));

    htable_put(&e->h_pfids, fmfe, fmfe);

    if (mfentry_persist(fmfe) != 0)
        goto out;

    status = 0;
out:
    if (to_mfkey) {
        if (to_mfkey->name) {

            free(to_mfkey->name);
        }
        free(to_mfkey);
    }
    return status;
}

int64_t export_read(export_t * e, uuid_t fid, uint64_t off, uint32_t len) {
    int64_t read = -1;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (off > mfe->attrs.size) {
        errno = 0;
        goto out;
    }

    if ((mfe->attrs.atime = time(NULL)) == -1)
        goto out;

    if (fsetxattr
            (mfe->fd, EATTRSTKEY, &mfe->attrs, sizeof (mattr_t),
            XATTR_REPLACE) != 0) {
        severe("export_read failed: fsetxattr in file %s failed: %s",
                mfe->path, strerror(errno));
        goto out;
    }
    read = off + len < mfe->attrs.size ? len : mfe->attrs.size - off;
out:

    return read;
}

int export_read_block(export_t * e, uuid_t fid, uint64_t bid, uint32_t n,
        dist_t * d) {
    int status = -1;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;
    int nrb = 0;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if ((nrb =
            pread(mfe->fd, d, n * sizeof (dist_t),
            bid * sizeof (dist_t))) != n * sizeof (dist_t)) {
        severe
                ("export_read_block failed: (bid: %lu, n: %u) pread in file %s failed: %s just %d bytes for %d blocks ",
                bid, n, mfe->path, strerror(errno), nrb, n);
        goto out;
    }

    status = 0;
out:

    return status;
}

int64_t export_write(export_t * e, uuid_t fid, uint64_t off, uint32_t len) {
    int64_t written = -1;
    uint64_t size;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (off + len > mfe->attrs.size) {
        size = off + len;

        if (export_update_blocks
                (e,
                (((off + len - mfe->attrs.size) + ROZOFS_BSIZE -
                1) / ROZOFS_BSIZE)) != 0)
            goto out;

        mfe->attrs.size = off + len;
    }

    mfe->attrs.mtime = mfe->attrs.ctime = time(NULL);

    if (fsetxattr
            (mfe->fd, EATTRSTKEY, &mfe->attrs, sizeof (mattr_t),
            XATTR_REPLACE) != 0) {
        severe("export_write failed: fsetxattr in file %s failed: %s",
                mfe->path, strerror(errno));
        goto out;
    }

    written = len;

out:
    return written;
}

int export_write_block(export_t * e, uuid_t fid, uint64_t bid, uint32_t n,
        dist_t d) {
    int status = -1;
    mfentry_t *mfe = 0;
    off_t count;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    for (count = 0; count < n; count++) {
        if (pwrite
                (mfe->fd, &d, 1 * sizeof (dist_t),
                ((off_t) bid + count) * (off_t) sizeof (dist_t)) !=
                1 * sizeof (dist_t)) {
            severe("export_write_block failed: pwrite in file %s failed: %s",
                    mfe->path, strerror(errno));
            goto out;
        }
    }
    status = 0;
out:

    return status;
}

int export_readdir(export_t * e, fid_t fid, child_t ** children) {
    int status = -1;
    mfentry_t *mfe = 0;
    DIR *dp;
    struct dirent *ep;
    child_t **iterator;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (!(dp = opendir(mfe->path))) {
        goto out;
    }

    iterator = children;

    while ((ep = readdir(dp)) != 0) {
        if (strcmp(ep->d_name, e->trashname) == 0) {
            continue;
        }
        *iterator = xmalloc(sizeof (child_t)); // XXX FREE?
        (*iterator)->name = xstrdup(ep->d_name); // XXX FREE?
        iterator = &(*iterator)->next;
    }

    if (closedir(dp) == -1)
        goto out;

    mfe->attrs.atime = time(NULL);

    if (mfentry_persist(mfe) != 0) {
        goto out;
    }

    *iterator = NULL;
    status = 0;
out:
    return status;
}

int export_open(export_t * e, fid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    int flag;
    DEBUG_FUNCTION;

    flag = O_RDWR;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (mfe->fd == -1) {

        if ((mfe->fd = open(mfe->path, flag)) < 0) {
            severe("export_open failed for file %s: %s", mfe->path,
                    strerror(errno));
            goto out;
        }
    }

    mfe->cnt++;

    status = 0;
out:

    return status;
}

int export_close(export_t * e, fid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }

    if (mfe->cnt == 1) {

        if (close(mfe->fd) != 0) {
            goto out;
        }
        mfe->fd = -1;
    }

    mfe->cnt--;

    status = 0;
out:
    return status;
}
