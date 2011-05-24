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
#define FUSE_USE_VERSION 26

#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "rozo.h"
#include "config.h"
#include "log.h"
#include "file.h"
#include "htable.h"
#include "xmalloc.h"

#define hash_xor8(n)    (((n) ^ ((n)>>8) ^ ((n)>>16) ^ ((n)>>24)) & 0xff)
#define INODE_HSIZE 256
#define PATH_HSIZE  256

typedef struct ientry {
    fuse_ino_t inode;
    fid_t fid;
    list_t list;
} ientry_t;

static char host[255];
static char export[255];
static exportclt_t exportclt;

static htable_t htable_inode;
static htable_t htable_fid;

static list_t inode_entries;

static fuse_ino_t inode_idx = 1;

static void ientries_release() {
    list_t *p, *q;

    DEBUG_FUNCTION;

    htable_release(&htable_inode);
    htable_release(&htable_fid);

    list_for_each_forward_safe(p, q, &inode_entries) {
        ientry_t *entry = list_entry(p, ientry_t, list);
        list_remove(p);
        free(entry);
    }
}

static void put_ientry(ientry_t * ie) {
    DEBUG_FUNCTION;
    DEBUG("put inode: %lu\n", ie->inode);
    htable_put(&htable_inode, &ie->inode, ie);
    htable_put(&htable_fid, ie->fid, ie);
    list_push_front(&inode_entries, &ie->list);
}

static void del_ientry(ientry_t * ie) {
    DEBUG_FUNCTION;
    DEBUG("del inode: %lu\n", ie->inode);
    htable_del(&htable_inode, &ie->inode);
    htable_del(&htable_fid, ie->fid);
    list_remove(&ie->list);
}

static uint32_t fuse_ino_hash(void *n) {
    return hash_xor8(*(uint32_t *) n);
}

static int fuse_ino_cmp(void *v1, void *v2) {
    return (*(fuse_ino_t *) v1 - *(fuse_ino_t *) v2);
}

static int fid_cmp(void *key1, void *key2) {
    return memcmp(key1, key2, sizeof (fid_t));
}

static unsigned int fid_hash(void *key) {
    uint32_t hash = 0;
    uint8_t *c;
    for (c = key; c != key + 16; c++)
        hash = *c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

static struct stat *mattr_to_stat(mattr_t * attr, struct stat *st) {
    memset(st, 0, sizeof (struct stat));
    st->st_mode = attr->mode;
    st->st_nlink = attr->nlink;
    st->st_size = attr->size;
    st->st_ctime = attr->ctime;
    st->st_atime = attr->atime;
    st->st_mtime = attr->mtime;
    st->st_blksize = ROZO_BSIZE;
    st->st_blocks = (attr->size) / 512 + 1; //XXX WRONG
    st->st_dev = 0;
    st->st_gid = getgid();
    st->st_uid = getuid();
    return st;
}

static mattr_t *stat_to_mattr(struct stat *st, mattr_t * attr, int to_set) {
    if (to_set & FUSE_SET_ATTR_MODE)
        attr->mode = st->st_mode;
    //attr->nlink = st->st_nlink;
    if (to_set & FUSE_SET_ATTR_SIZE)
        attr->size = st->st_size;
    //attr->ctime = st->st_ctime;
    if (to_set & FUSE_SET_ATTR_ATIME)
        attr->atime = st->st_atime;
    if (to_set & FUSE_SET_ATTR_MTIME)
        attr->mtime = st->st_mtime;
    return attr;
}

void rozofs_ll_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
        mode_t mode, dev_t rdev) {
    ientry_t *ie = 0;
    ientry_t *nie = 0;
    mattr_t attrs;
    struct fuse_entry_param fep;
    struct stat stbuf;
    DEBUG_FUNCTION;

    DEBUG("mknod (%lu,%s,%04o,%08lX)\n", (unsigned long int) parent, name,
            (unsigned int) mode, (unsigned long int) rdev);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_mknod(&exportclt, ie->fid, (char *) name, mode, &attrs) !=
            0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (!(nie = htable_get(&htable_fid, attrs.fid))) {
        nie = xmalloc(sizeof (ientry_t));
        memcpy(nie->fid, attrs.fid, sizeof (fid_t));
        nie->inode = inode_idx++;
        list_init(&nie->list);
        put_ientry(nie);
    }
    memset(&fep, 0, sizeof (fep));
    fep.ino = nie->inode;
    fep.attr_timeout = 0.0;
    fep.entry_timeout = 0.0;
    memcpy(&fep.attr, mattr_to_stat(&attrs, &stbuf), sizeof (struct stat));
    fuse_reply_entry(req, &fep);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
        mode_t mode) {
    ientry_t *ie = 0;
    ientry_t *nie = 0;
    mattr_t attrs;
    struct fuse_entry_param fep;
    struct stat stbuf;
    DEBUG_FUNCTION;

    DEBUG("mkdir (%lu,%s,%04o)\n", (unsigned long int) parent, name,
            (unsigned int) mode);

    mode = (mode | S_IFDIR);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_mkdir(&exportclt, ie->fid, (char *) name, mode, &attrs) !=
            0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (!(nie = htable_get(&htable_fid, (attrs.fid)))) {
        nie = xmalloc(sizeof (ientry_t));
        memcpy(nie->fid, attrs.fid, sizeof (fid_t));
        nie->inode = inode_idx++;
        list_init(&nie->list);
        put_ientry(nie);
    }

    memset(&fep, 0, sizeof (fep));
    fep.ino = nie->inode;
    fep.attr_timeout = 0.0;
    fep.entry_timeout = 0.0;
    memcpy(&fep.attr, mattr_to_stat(&attrs, &stbuf), sizeof (struct stat));
    fuse_reply_entry(req, &fep);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
        fuse_ino_t newparent, const char *newname) {
    ientry_t *pie = 0;
    ientry_t *npie = 0;
    mattr_t attrs;
    DEBUG_FUNCTION;

    DEBUG("rename (%lu,%s,%lu,%s)\n", (unsigned long int) parent, name,
            (unsigned long int) newparent, newname);

    if (strlen(name) > ROZO_FILENAME_MAX ||
            strlen(newname) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(pie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (!(npie = htable_get(&htable_inode, &newparent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_lookup(&exportclt, pie->fid, (char *) name, &attrs) != 0) {
        if (errno == ESTALE) {
            del_ientry(pie);
            free(pie);
            errno = ESTALE;
        }
        goto error;
    }
    if (exportclt_rename(&exportclt, attrs.fid, npie->fid, (char *) newname)
            != 0) {
        if (errno == ESTALE) {
            // XXX Only one of them might be stale
            // cache might (WILL) be inconsistent !!!
            del_ientry(pie);
            del_ientry(npie);
            free(pie);
            free(npie);
            errno = ESTALE;
        }
        goto error;
    }
    fuse_reply_err(req, 0);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_readlink(fuse_req_t req, fuse_ino_t ino) {
    char *target = 0;
    ientry_t *ie = NULL;
    DEBUG_FUNCTION;

    DEBUG("readlink (%lu)\n", (unsigned long int) ino);

    if ((ie = htable_get(&htable_inode, &ino)) != NULL) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_readlink(&exportclt, ie->fid, target) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    fuse_reply_readlink(req, (char *) target);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    file_t *file;
    ientry_t *ie = 0;
    DEBUG_FUNCTION;

    DEBUG("open (%lu)\n", (unsigned long int) ino);

    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }
    if (!(file = file_open(&exportclt, ie->fid, S_IRWXU))) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    fi->fh = (unsigned long) file;
    fuse_reply_open(req, fi);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
        struct fuse_file_info *fi) {
    size_t length = 0;
    char *buff;
    ientry_t *ie = 0;

    DEBUG_FUNCTION;

    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }

    file_t* file = (file_t *) (unsigned long) fi->fh;

    memcpy(file->fid, ie->fid, sizeof (fid_t));

    buff = 0;
    length = file_read(file, off, &buff, size);
    if (length == -1)
        goto error;
    fuse_reply_buf(req, (char *) buff, length);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
        size_t size, off_t off, struct fuse_file_info *fi) {
    size_t length = 0;
    ientry_t *ie = 0;
    DEBUG_FUNCTION;

    DEBUG("write to inode %lu %llu bytes at position %llu\n",
            (unsigned long int) ino, (unsigned long long int) size,
            (unsigned long long int) off);

    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }

    file_t* file = (file_t *) (unsigned long) fi->fh;
    memcpy(file->fid, ie->fid, sizeof (fid_t));

    length = file_write(file, off, buf, size);
    if (length == -1)
        goto error;
    fuse_reply_write(req, length);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_flush(fuse_req_t req, fuse_ino_t ino,
        struct fuse_file_info *fi) {
    file_t *f;
    ientry_t *ie = 0;
    DEBUG_FUNCTION;

    DEBUG("flush (%lu)\n", (unsigned long int) ino);

    // Sanity check
    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }

    if (!(f = (file_t *) (unsigned long) fi->fh)) {
        errno = EBADF;
        goto out;
    }

    memcpy(f->fid, ie->fid, sizeof (fid_t));

    if (file_flush(f) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    fuse_reply_err(req, 0);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_access(fuse_req_t req, fuse_ino_t ino, int mask) {
    DEBUG_FUNCTION;
    fuse_reply_err(req, 0);
}

void rozofs_ll_release(fuse_req_t req, fuse_ino_t ino,
        struct fuse_file_info *fi) {

    DEBUG_FUNCTION;
    DEBUG("release (%lu)\n", (unsigned long int) ino);

    file_close((file_t *) ((unsigned long) fi->fh));
    fuse_reply_err(req, 0);
    return;
}

void rozofs_ll_statfs(fuse_req_t req, fuse_ino_t ino) {
    (void) ino;
    estat_t estat;
    struct statvfs st;
    DEBUG_FUNCTION;

    memset(&st, 0, sizeof (struct statvfs));
    if (exportclt_stat(&exportclt, &estat) == -1)
        goto error;

    st.f_blocks = estat.blocks; // + estat.bfree;
    st.f_bavail = st.f_bfree = estat.bfree;
    st.f_frsize = st.f_bsize = estat.bsize;
    st.f_favail = st.f_ffree = estat.ffree;
    st.f_files = estat.files;
    st.f_namemax = estat.namemax;

    fuse_reply_statfs(req, &st);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
        struct fuse_file_info *fi) {
    struct stat stbuf;
    (void) fi;
    ientry_t *ie = 0;
    mattr_t attr;
    DEBUG_FUNCTION;

    DEBUG("getattr for inode: %lu\n", (unsigned long int) ino);
    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }

    if (exportclt_getattr(&exportclt, ie->fid, &attr) == -1) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }

    mattr_to_stat(&attr, &stbuf);

    stbuf.st_ino = ino;

    fuse_reply_attr(req, &stbuf, 0.0);

    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *stbuf,
        int to_set, struct fuse_file_info *fi) {
    ientry_t *ie = 0;
    struct stat o_stbuf;
    mattr_t attr;
    DEBUG_FUNCTION;

    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_getattr(&exportclt, ie->fid, &attr) == -1) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (exportclt_setattr(&exportclt, ie->fid, stat_to_mattr(stbuf, &attr, to_set)) == -1) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }

    mattr_to_stat(&attr, &o_stbuf);
    o_stbuf.st_ino = ino;

    fuse_reply_attr(req, &o_stbuf, 0.0);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

// XXX TODO

/* Can't find a fid from path !!!!!!!!!!
void rozofs_ll_symlink(fuse_req_t req, const char *link, fuse_ino_t parent,
        const char *name) {
    struct fuse_entry_param e;
    char path[ROZO_PATH_MAX];
    uint32_t name_len;
    struct stat stbuf;

    DEBUG_FUNCTION;

    DEBUG("symlink (%s,%lu,%s)\n", link, (unsigned long int) parent, name);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }

    exportclt_symlink(exportclt, )
    if (rozo_client_symlink(&rozo_client, link, inode_map(parent, name, path)) == -1) {
        fuse_reply_err(req, errno);
    } else {
        memset(&e, 0, sizeof (e));
        ientry_t * ie = NULL;
        ie = get_inode_by_fid(inode_map(parent, name, path));
        e.ino = ie->inode;
        e.attr_timeout = 0.0;
        e.entry_timeout = 0.0;
        if (rozo_client_stat(&rozo_client, ie->path, &stbuf) == -1) {
            fuse_reply_err(req, errno);
        }
        memcpy(&e.attr, &stbuf, sizeof (struct stat));
        fuse_reply_entry(req, &e);
    }
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}
 */

void rozofs_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    ientry_t *ie = 0;
    ientry_t *ie2 = 0;
    mattr_t attrs;
    DEBUG_FUNCTION;

    DEBUG("rmdir (%lu,%s)\n", (unsigned long int) parent, name);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_lookup(&exportclt, ie->fid, (char *) name, &attrs) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (exportclt_rmdir(&exportclt, attrs.fid) != 0) {
        goto error;
    }
    if ((ie2 = htable_get(&htable_fid, attrs.fid))) {
        del_ientry(ie2);
        free(ie2);
    }
    fuse_reply_err(req, 0);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
    ientry_t *ie = 0;
    ientry_t *ie2 = 0;
    mattr_t attrs;
    DEBUG_FUNCTION;

    DEBUG("unlink (%lu,%s)\n", (unsigned long int) parent, name);

    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_lookup(&exportclt, ie->fid, (char *) name, &attrs) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (exportclt_unlink(&exportclt, attrs.fid) != 0) {
        goto error;
    }
    if ((ie2 = htable_get(&htable_fid, attrs.fid))) {
        del_ientry(ie2);
        free(ie2);
    }
    fuse_reply_err(req, 0);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

struct dirbuf {
    char *p;
    size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
        fuse_ino_t ino, mattr_t * attrs) {
    struct stat stbuf;
    size_t oldsize = b->size;
    b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
    b->p = (char *) realloc(b->p, b->size);
    memcpy(&stbuf, mattr_to_stat(attrs, &stbuf), sizeof (struct stat));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
            b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
        off_t off, size_t maxsize) {
    if (off < bufsize)
        return fuse_reply_buf(req, buf + off, min(bufsize - off, maxsize));
    else
        return fuse_reply_buf(req, NULL, 0);
}

void rozofs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
        struct fuse_file_info *fi) {
    ientry_t *ie = 0;
    child_t *child, *iterator, *free_it;
    struct dirbuf b;
    DEBUG_FUNCTION;

    if (!(ie = htable_get(&htable_inode, &ino))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_readdir(&exportclt, ie->fid, &child) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    memset(&b, 0, sizeof (b));
    iterator = child;
    // TODO ? could be optimized by adding fid in child_t
    while (iterator != NULL) {
        mattr_t attrs;
        ientry_t *ie2 = 0;
        if (exportclt_lookup(&exportclt, ie->fid, iterator->name, &attrs) !=
                0) {
            if (errno == ESTALE) {
                del_ientry(ie);
                free(ie);
                errno = ESTALE;
                goto error;
            }
            iterator = iterator->next;
            continue;
        }
        // May be already cached
        if (!(ie2 = htable_get(&htable_fid, attrs.fid))) {
            // If not cache it
            ie2 = xmalloc(sizeof (ientry_t));
            memcpy(ie2->fid, attrs.fid, sizeof (fid_t));
            ie2->inode = inode_idx++;
            list_init(&ie2->list);
            put_ientry(ie2);
        }
        // put it on the request
        dirbuf_add(req, &b, iterator->name, ie2->inode, &attrs);
        free_it = iterator;
        iterator = iterator->next;
        free(free_it->name);
        free(free_it);
    }

    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    struct fuse_entry_param fep;
    ientry_t *ie = 0;
    ientry_t *nie = 0;
    struct stat stbuf;
    mattr_t attrs;
    DEBUG_FUNCTION;

    DEBUG("lookup (%lu,%s)\n", (unsigned long int) parent, name);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_lookup(&exportclt, ie->fid, (char *) name, &attrs) != 0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;

    }
    if (!(nie = htable_get(&htable_fid, attrs.fid))) {
        nie = xmalloc(sizeof (ientry_t));
        memcpy(nie->fid, attrs.fid, sizeof (fid_t));
        nie->inode = inode_idx++;
        list_init(&nie->list);
        put_ientry(nie);
    }
    memset(&fep, 0, sizeof (fep));
    fep.ino = nie->inode;
    fep.attr_timeout = 0.0;
    fep.entry_timeout = 0.0;
    memcpy(&fep.attr, mattr_to_stat(&attrs, &stbuf), sizeof (struct stat));

    fuse_reply_entry(req, &fep);
    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

void rozofs_ll_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
    ientry_t *ie = 0;
    ientry_t *nie = 0;
    mattr_t attrs;
    struct fuse_entry_param fep;
    struct stat stbuf;
    file_t *file;
    DEBUG_FUNCTION;

    warning("create (%lu,%s,%04o)\n", (unsigned long int) parent, name, (unsigned int) mode);

    if (strlen(name) > ROZO_FILENAME_MAX) {
        errno = ENAMETOOLONG;
        goto error;
    }
    if (!(ie = htable_get(&htable_inode, &parent))) {
        errno = ENOENT;
        goto error;
    }
    if (exportclt_mknod(&exportclt, ie->fid, (char *) name, mode, &attrs) !=
            0) {
        if (errno == ESTALE) {
            del_ientry(ie);
            free(ie);
            errno = ESTALE;
        }
        goto error;
    }
    if (!(nie = htable_get(&htable_fid, attrs.fid))) {
        nie = xmalloc(sizeof (ientry_t));
        memcpy(nie->fid, attrs.fid, sizeof (fid_t));
        nie->inode = inode_idx++;
        list_init(&nie->list);
        put_ientry(nie);
    }

    if (!(file = file_open(&exportclt, nie->fid, S_IRWXU))) {
        if (errno == ESTALE) {
            del_ientry(nie);
            free(nie);
            errno = ESTALE;
        }
        goto error;
    }

    memset(&fep, 0, sizeof (fep));
    fep.ino = nie->inode;
    fep.attr_timeout = 0.0;
    fep.entry_timeout = 0.0;

    memcpy(&fep.attr, mattr_to_stat(&attrs, &stbuf), sizeof (struct stat));


    fi->fh = (unsigned long) file;
    fuse_reply_create(req, &fep, fi);

    goto out;
error:
    fuse_reply_err(req, errno);
out:
    return;
}

static struct fuse_lowlevel_ops rozo_ll_operations = {
    //.init = rozofs_ll_init,
    //.destroy = rozofs_ll_destroy,
    .lookup = rozofs_ll_lookup,
    //.forget = rozofs_ll_forget,
    .getattr = rozofs_ll_getattr,
    .setattr = rozofs_ll_setattr,
    .readlink = rozofs_ll_readlink,
    .mknod = rozofs_ll_mknod,
    .mkdir = rozofs_ll_mkdir,
    .unlink = rozofs_ll_unlink,
    .rmdir = rozofs_ll_rmdir,
    //.symlink = rozofs_ll_symlink,
    .rename = rozofs_ll_rename,
    .open = rozofs_ll_open,
    //.link = rozofs_ll_link,
    .read = rozofs_ll_read,
    .write = rozofs_ll_write,
    .flush = rozofs_ll_flush,
    .release = rozofs_ll_release,
    //.opendir = rozofs_ll_opendir,
    .readdir = rozofs_ll_readdir,
    //.releasedir = rozofs_ll_releasedir,
    //.fsyncdir = rozofs_ll_fsyncdir,
    //.chmod = rozofs_ll_chmod,
    .statfs = rozofs_ll_statfs,
    //.setxattr = rozofs_ll_setxattr,
    //.getxattr = rozofs_ll_getxattr,
    //.listxattr = rozofs_ll_listxattr,
    //.removexattr = rozofs_ll_removexattr,
    //.access = rozofs_ll_access,
    //.create = rozofs_ll_create,
    //.getlk = rozofs_ll_getlk,
    //.setlk = rozofs_ll_setlk,
    //.bmap = rozofs_ll_bmap,
    //.ioctl = rozofs_ll_ioctl,
    //.poll = rozofs_ll_poll,
};

static void usage() {
    printf("Rozo fuse mounter - %s\n", VERSION);
    printf
            ("Usage: rozofs {--help | -h} | {export_host export_path mount_point} \n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf
            ("\texport_host\taddress (or dns name) where exportd deamon is running.\n");
    printf("\texport_path\tname of an export see exportd.\n");
    printf("\tmount_point\tdirectory where filesystem will be mounted.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    struct fuse_args args; // = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    char *mountpoint;
    int err = -1;
    char c;
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'h':
            usage();
            break;
        case '?':
            usage();
        default:
            exit(EXIT_FAILURE);
        }
    }

    if (argc < 4) {
        usage();
    }

    strcpy(host, argv[1]);
    strcpy(export, argv[2]);

    args.argc = 7;
    args.argv = xmalloc(8 * sizeof (char *));
    args.argv[0] = xstrdup(argv[0]);
    args.argv[1] = xstrdup("-s");
    args.argv[2] = xstrdup("-oallow_other");
    args.argv[3] = xstrdup("-ofsname=rozo");
    args.argv[4] = xstrdup("-osubtype=rozo");
    // FUSE 2.8 REQUIERED
    args.argv[5] = xstrdup("-obig_writes");
    //args.argv[6] = xstrdup("-d"); // XXX DEBUG
    args.argv[6] = xstrdup(argv[3]);
    args.argv[7] = 0;

    openlog("rozomount", LOG_PID, LOG_LOCAL0);

    list_init(&inode_entries);
    htable_initialize(&htable_inode, INODE_HSIZE, fuse_ino_hash,
            fuse_ino_cmp);
    htable_initialize(&htable_fid, PATH_HSIZE, fid_hash, fid_cmp);

    if (exportclt_initialize(&exportclt, host, export) != 0) {
        perror("Fail to initialize export client");
        return EXIT_FAILURE;
    }

    ientry_t *root = xmalloc(sizeof (ientry_t));
    memcpy(root->fid, exportclt.rfid, sizeof (fid_t));
    root->inode = inode_idx++;
    put_ientry(root);

    info("mounting - export: %s from : %s on: %s", argv[2], argv[1], argv[3]);
    if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
            (ch = fuse_mount(mountpoint, &args)) != NULL) {
        struct fuse_session *se;

        se = fuse_lowlevel_new(&args, &rozo_ll_operations,
                sizeof (rozo_ll_operations), NULL);
        if (se != NULL) {
            if (fuse_set_signal_handlers(se) != -1) {
                fuse_session_add_chan(se, ch);
                err = fuse_session_loop(se);
                fuse_remove_signal_handlers(se);
                fuse_session_remove_chan(ch);
            }
            fuse_session_destroy(se);
        }
        fuse_unmount(mountpoint, ch);
    }
    fuse_opt_free_args(&args);
    exportclt_release(&exportclt);
    ientries_release();
    rozo_release();
    return err ? 1 : 0;
}
