/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozo.

  Rozo is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozo is distributed in the hope that it will be useful, but
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
#include "rozo.h"
#include "volume.h"

#define EHSIZE 2048

#define EBLOCKSKEY	"user.rozo.export.blocks"
#define EFILESKEY	"user.rozo.export.files"
#define EVERSIONKEY	"user.rozo.export.version"
#define EATTRSTKEY	"user.rozo.export.file.attrs"

static inline char *export_map(export_t * e, const char *vpath, char *path) {
    strcpy(path, e->root);
    strcat(path, vpath);
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
    char *path;                 // Absolute path on underlying fs
    char *name;                 // Name of file
    int fd;                     // File descriptor
    uint16_t cnt;               // Open counter
    mattr_t attrs;              // meta file attr
    list_t list;
} mfentry_t;

// Key to search an mfentry with its parent and its name

typedef struct mfkey {
    fid_t pfid;                 // UUID of the parent
    char *name;                 // Name of entry
} mfkey_t;

static int mfentry_initialize(mfentry_t * mfe, mfentry_t * parent,
                              const char *name, char *path) {
    int status = -1;
    DEBUG_FUNCTION;

    mfe->parent = parent;
    mfe->path = xstrdup(path);
    mfe->name = xstrdup(name);

    if (getxattr(path, EATTRSTKEY, &(mfe->attrs), sizeof (mattr_t)) == -1)
        goto error;

    // Open the fd is not necessary now
    mfe->fd = -1;
    // The counter is initialized to zero
    mfe->cnt = 0;

    list_init(&mfe->list);
    status = 0;
    goto out;
error:
    free(mfe->path);
    free(mfe->name);
out:
    return status;
}

static void mfentry_release(mfentry_t * mfe) {
    if (mfe) {
        if (mfe->fd != -1)
            close(mfe->fd);
        free(mfe->path);
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
    mfkey_t *mfk = (mfkey_t *) key;
    uint32_t hash = 0;
    uint8_t *c;
    char *d;

    for (c = mfk->pfid; c != mfk->pfid + 16; c++)
        hash = *c + (hash << 6) + (hash << 16) - hash;
    for (d = mfk->name; *d != '\0'; d++)
        hash = *d + (hash << 6) + (hash << 16) - hash;
    return hash;
}

static int mfentry_cmp_fid(void *k1, void *k2) {
    return memcmp(k1, k2, sizeof (fid_t));
}

static int mfentry_cmp_fid_name(void *k1, void *k2) {

    mfkey_t *sk1 = (mfkey_t *) k1;
    mfkey_t *sk2 = (mfkey_t *) k2;

    if ((uuid_compare(sk1->pfid, sk2->pfid) == 0) &&
        (strcmp(sk1->name, sk2->name) == 0)) {
        return 0;
    } else {
        return 1;
    }
}

static void export_put_mfentry(export_t * e, mfentry_t * mfe) {
    DEBUG_FUNCTION;
    mfkey_t *mfkey = xmalloc(sizeof (mfentry_t));

    // Consideration for root node
    if (mfe->parent != NULL) {
        memcpy(mfkey->pfid, mfe->parent->attrs.fid, sizeof (fid_t));
    } else {
        uuid_clear(mfkey->pfid);
    }
    mfkey->name = xstrdup(mfe->name);

    htable_put(&e->hfids, mfe->attrs.fid, mfe);
    htable_put(&e->h_pfids, mfkey, mfe);

    list_push_front(&e->mfiles, &mfe->list);
}

static void export_del_mfentry(export_t * e, mfentry_t * mfe) {
    DEBUG_FUNCTION;
    mfkey_t *mfkey = xmalloc(sizeof (mfentry_t));

    memcpy(mfkey->pfid, mfe->parent->attrs.fid, sizeof (fid_t));
    mfkey->name = xstrdup(mfe->name);

    htable_del(&e->hfids, mfe->attrs.fid);
    htable_del(&e->h_pfids, mfkey);

    list_remove(&mfe->list);
}

static inline int export_update_files(export_t * e, int32_t n) {
    int status = -1;
    uint64_t files;

    if (getxattr(e->root, EFILESKEY, &files, sizeof (uint64_t)) == -1)
        goto out;
    files += n;
    if (setxattr(e->root, EFILESKEY, &files, sizeof (uint64_t), XATTR_REPLACE)
        == -1)
        goto out;
    status = 0;
out:
    return status;
}

static inline int export_update_blocks(export_t * e, int32_t n) {
    int status = -1;
    uint64_t blocks;
    DEBUG_FUNCTION;

    if (getxattr(e->root, EBLOCKSKEY, &blocks, sizeof (uint64_t)) !=
        sizeof (uint64_t))
        goto out;
    blocks += n;
    if (setxattr
        (e->root, EBLOCKSKEY, &blocks, sizeof (uint64_t), XATTR_REPLACE) != 0)
        goto out;
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
    memset(attrs.sids, 0, ROZO_SAFE_MAX * sizeof (sid_t));
    attrs.mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
    attrs.nlink = 2;
    if ((attrs.ctime = attrs.atime = attrs.mtime = time(NULL)) == -1)
        goto out;
    attrs.size = ROZO_DIR_SIZE;
    if (setxattr(path, EATTRSTKEY, &attrs, sizeof (mattr_t), XATTR_CREATE)
        != 0)
        goto out;

    status = 0;
out:
    return status;
}

int export_initialize(export_t * e, uint32_t eid, const char *root) {
    int status = -1;
    mfentry_t *mfe;
    DEBUG_FUNCTION;

    if (!realpath(root, e->root))
        goto out;
    if (export_check_root(e->root) != 0)
        goto out;
    if (export_check_setup(e->root) != 0)
        goto out;

    e->eid = eid;
    list_init(&e->mfiles);
    htable_initialize(&e->hfids, EHSIZE, mfentry_hash_fid, mfentry_cmp_fid);
    htable_initialize(&e->h_pfids, EHSIZE, mfentry_hash_fid_name,
                      mfentry_cmp_fid_name);

    // Register the root
    mfe = xmalloc(sizeof (mfentry_t));
    if (mfentry_initialize(mfe, 0, e->root, e->root) != 0)
        goto out;

    export_put_mfentry(e, mfe);
    memcpy(e->rfid, mfe->attrs.fid, sizeof (fid_t));

    status = 0;
out:
    return status;
}

void export_release(export_t * e) {
    list_t *p, *q;
    DEBUG_FUNCTION;

    list_for_each_forward_safe(p, q, &e->mfiles) {
        mfentry_t *mfe = list_entry(p, mfentry_t, list);
        list_remove(&mfe->list);
        mfentry_release(mfe);
        free(mfe);
    }
    htable_release(&e->hfids);
    htable_release(&e->h_pfids);
}

int export_stat(export_t * e, estat_t * st) {
    int status = -1;
    struct statfs stfs;
    volume_stat_t vstat;
    DEBUG_FUNCTION;

    st->bsize = ROZO_BSIZE;
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
    mfkey_t *mfkey = 0;
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

    strcpy(path, pmfe->path);
    strcat(path, "/");
    strcat(path, name);

    mfkey = xmalloc(sizeof (mfkey_t));
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

    if (mfe->attrs.size != attrs->size) {

        uint64_t nrb_new = ((attrs->size + ROZO_BSIZE - 1) / ROZO_BSIZE);
        uint64_t nrb_old = ((mfe->attrs.size + ROZO_BSIZE - 1) / ROZO_BSIZE);

        // Open the file descriptor
        if ((fd = open(mfe->path, O_RDWR)) < 0) {
            severe("export_setattr failed: open for file %s failed: %s",
                   mfe->path, strerror(errno));
            goto out;
        }
        if (mfe->attrs.size > attrs->size) {
            if (ftruncate(fd, attrs->size * sizeof (dist_t)) != 0)
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
    mfe->attrs.ctime = attrs->ctime;
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
    mfkey_t *mfkey = 0;
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
    attrs->nlink = 0;
    if ((attrs->ctime = attrs->atime = attrs->mtime = time(NULL)) == -1)
        goto error;

    attrs->size = 0;
    if (setxattr(path, EATTRSTKEY, attrs, sizeof (mattr_t), XATTR_CREATE) !=
        0)
        goto error;

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

    mfkey = xmalloc(sizeof (mfkey_t));
    memcpy(mfkey->pfid, pmfe->attrs.fid, sizeof (fid_t));
    mfkey->name = xstrdup(name);

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
    memset(attrs->sids, 0, ROZO_SAFE_MAX * sizeof (uint16_t));
    attrs->mode = mode;
    attrs->nlink = 2;

    if ((attrs->ctime = attrs->atime = attrs->mtime = time(NULL)) == -1)
        goto error;

    attrs->size = ROZO_BSIZE;

    if (setxattr(path, EATTRSTKEY, attrs, sizeof (mattr_t), XATTR_CREATE) !=
        0)
        goto error;

    if (export_update_files(e, 1) != 0)
        goto error;

    pmfe->attrs.nlink++;

    if (mfentry_persist(pmfe) != 0) {
        goto error;
    }

    mfe = xmalloc(sizeof (mfentry_t));

    if (mfentry_initialize(mfe, pmfe, name, path) != 0)
        goto out;

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
        rmdir(path);
    }
    errno = xerrno;
out:
    return status;
}

// TODO projections will be deleted by maintenance.

int export_unlink(export_t * e, uuid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    char path[PATH_MAX + NAME_MAX + 1];
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

    export_del_mfentry(e, mfe);

    mfentry_release(mfe);
    free(mfe);

    if (unlink(path) == -1)
        goto out;
    if (export_update_files(e, -1) != 0)
        goto out;

    if (!S_ISLNK(mode))
        if (export_update_blocks
            (e, -(((int64_t) size + ROZO_BSIZE - 1) / ROZO_BSIZE)) != 0)
            goto out;

    status = 0;
out:
    return status;
}

int export_rmdir(export_t * e, uuid_t fid) {
    int status = -1;
    mfentry_t *mfe = 0;
    char path[PATH_MAX + NAME_MAX + 1];
    mode_t mode;
    DEBUG_FUNCTION;

    if (!(mfe = htable_get(&e->hfids, fid))) {
        errno = ESTALE;
        goto out;
    }
    strcpy(path, mfe->path);
    mode = mfe->attrs.mode;

    // Update the nlink of parent
    mfe->parent->attrs.nlink--;
    export_del_mfentry(e, mfe);
    mfentry_release(mfe);
    free(mfe);
    if (rmdir(path) == -1)
        goto out;
    if (export_update_files(e, -1) != 0)
        goto out;
    if (!S_ISLNK(mode))
        if (export_update_blocks(e, ROZO_DIR_SIZE / ROZO_BSIZE) != 0)
            goto out;

    status = 0;
out:
    return status;
}

/*
int export_symlink(export_t * e, uuid_t target, uuid_t parent,
        const char *name, mattr_t * attrs) {
    int status = -1;
    mfentry_t *tmfe = 0;
    mfentry_t *pmfe = 0;
    mfentry_t *lmfe = 0;
    char to[PATH_MAX + NAME_MAX + 1];
    DEBUG_FUNCTION;

    if (!(tmfe = htable_get(&e->hfids, target))) {
        errno = ESTALE;
        goto out;
    }
    if (!(pmfe = htable_get(&e->hfids, parent))) {
        errno = ESTALE;
        goto out;
    }

    strcpy(to, pmfe->path);
    strcat(to, "/");
    strcat(to, name);
    status = link(tmfe->path, to);
    lmfe = xmalloc(sizeof (mfentry_t));
    uuid_generate(lmfe->attrs.fid);
    lmfe->attrs.cid = 0;
    memset(&lmfe->attrs.sids, 0, ROZO_SAFE_MAX * sizeof (uint16_t));
    lmfe->attrs.mode = S_IFLNK | S_IRUSR | S_IWUSR | S_IXUSR;
    lmfe->attrs.nlink = 0;
    if ((lmfe->attrs.ctime = lmfe->attrs.atime = lmfe->attrs.mtime =
            time(NULL)) == -1)
        goto out;
    lmfe->attrs.size = strlen(tmfe->path) - strlen(rindex(tmfe->path, '/'));
    if (fsetxattr
            (lmfe->fd, EATTRSTKEY, &attrs, sizeof (mattr_t), XATTR_CREATE) != 0)
        goto out;
    // TO CHANGE
    //export_put_mfentry(e, lmfe);
    if (export_update_files(e, 1) != 0)
        goto out;

    memcpy(attrs, &lmfe->attrs, sizeof (mattr_t));
out:
    return status;
}
 */

int export_rename(export_t * e, uuid_t from, uuid_t parent, const char *name) {
    int status = -1;
    mfentry_t *fmfe = 0;
    mfentry_t *pmfe = 0;
    mfentry_t *ofmfe = 0;
    mfkey_t *to_mfkey = 0;
    mfkey_t *from_mfkey = 0;
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

    // rename
    if (rename(fmfe->path, to) == -1)
        goto out;

    // Prepare the NEW key
    to_mfkey = xmalloc(sizeof (mfkey_t));
    memcpy(to_mfkey->pfid, pmfe->attrs.fid, sizeof (fid_t));
    to_mfkey->name = xstrdup(name);

    // Prepare the OLD key
    from_mfkey = xmalloc(sizeof (mfkey_t));
    memcpy(from_mfkey->pfid, fmfe->parent->attrs.fid, sizeof (fid_t));
    from_mfkey->name = xstrdup(fmfe->name);

    // If the target file or directory already existed
    if ((ofmfe = htable_get(&e->h_pfids, to_mfkey))) {

        // Delete the old newpath
        size = ofmfe->attrs.size;
        mode = ofmfe->attrs.mode;
        export_del_mfentry(e, ofmfe);
        mfentry_release(ofmfe);
        free(ofmfe);

        if (export_update_files(e, -1) != 0)
            goto out;

        if (!S_ISLNK(mode))
            if (export_update_blocks
                (e, -(((int64_t) size + ROZO_BSIZE - 1) / ROZO_BSIZE)) != 0)
                goto out;
    }
    // Change the path
    free(fmfe->path);
    fmfe->path = xstrdup(to);
    free(fmfe->name);
    fmfe->name = xstrdup(name);
    // Put the new parent
    fmfe->parent = pmfe;

    htable_del(&e->h_pfids, from_mfkey);
    htable_put(&e->h_pfids, to_mfkey, fmfe);

    status = 0;
out:
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
            ("export_read_block failed: pread in file %s failed: %s just %d bytes for %d blocks ",
             mfe->path, strerror(errno), nrb, n);
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
             (((off + len - mfe->attrs.size) + ROZO_BSIZE -
               1) / ROZO_BSIZE)) != 0)
            goto out;

        mfe->attrs.size = off + len;

        if (fsetxattr
            (mfe->fd, EATTRSTKEY, &mfe->attrs, sizeof (mattr_t),
             XATTR_REPLACE) != 0) {
            severe("export_write failed: fsetxattr in file %s failed: %s",
                   mfe->path, strerror(errno));
            goto out;
        }
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
        *iterator = xmalloc(sizeof (child_t));
        (*iterator)->name = xstrdup(ep->d_name);
        iterator = &(*iterator)->next;
    }

    if (closedir(dp) == -1)
        goto out;

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
