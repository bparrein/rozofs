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
#include <config.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <sys/vfs.h>
#include <uuid/uuid.h>

#include "log.h"
#include "volume.h"
#include "vfs.h"

#define VFS_UUID_XATTR_KEY "user.rozo.vfs.uuid"
#define VFS_BLOCKS_XATTR_KEY "user.rozo.vfs.blocks"
#define VFS_FILES_XATTR_KEY "user.rozo.vfs.files"
#define VFS_VERSION_XATTR_KEY "user.rozo.vfs.version"

#define VFS_MF_UUID_XATTR_KEY "user.rozo.vfs.mf.uuid"
#define VFS_MF_MPSS_XATTR_KEY "user.rozo.vfs.mf.mss"
#define VFS_MF_SIZE_XATTR_KEY "user.rozo.vfs.mf.size"

static inline char * vfs_map(vfs_t *vfs, const char *vpath, char *path) {

    DEBUG_FUNCTION;

    strcpy(path, vfs->root);
    strcat(path, vpath);

    return path;
}

static inline char * vfs_unmap(vfs_t *vfs, const char *path, char *vpath) {

    DEBUG_FUNCTION;

    strcpy(vpath, path + strlen(vfs->root));

    return vpath;
}

int vfs_version(const char *root, char *version) {
    int status = 0;
    struct stat st;

    DEBUG_FUNCTION;

    if (stat(root, &st) == -1) {
        status = -1;
        goto out;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
        goto out;
    }

    char *version_recup;

    if (getxattr(root, VFS_VERSION_XATTR_KEY, &version_recup, sizeof (char) * PATH_MAX) == -1) {
        status = -1;
        goto out;
    }

    strcpy(version, version_recup);

out:
    return status;
}

int vfs_create(const char *root) {

    int status = 0;
    uint64_t zero = 0;
    struct stat st;
    uuid_t uuid;

    DEBUG_FUNCTION;

    if (stat(root, &st) == -1) {
        status = -1;
        goto out;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
        goto out;
    }

    uuid_generate(uuid);

    if (setxattr(root, VFS_UUID_XATTR_KEY, uuid, sizeof (uuid_t), XATTR_CREATE) != 0) {
        status = -1;
        goto out;
    }

    if (setxattr(root, VFS_BLOCKS_XATTR_KEY, &zero, sizeof (uint64_t), XATTR_CREATE) != 0) {
        status = -1;
        goto out;
    }

    if (setxattr(root, VFS_FILES_XATTR_KEY, &zero, sizeof (uint64_t), XATTR_CREATE) != 0) {
        status = -1;
        goto out;
    }

    const char *version = VERSION;

    if (setxattr(root, VFS_VERSION_XATTR_KEY, &version, sizeof (char) * strlen(version) + 1, XATTR_CREATE) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int vfs_uuid(const char *root, uuid_t uuid) {

    int status = 0;
    struct stat st;

    DEBUG_FUNCTION;

    if (stat(root, &st) == -1) {
        status = -1;
        goto out;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
        goto out;
    }

    if (getxattr(root, VFS_UUID_XATTR_KEY, &uuid, sizeof (uuid_t)) == -1) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int vfs_statfs(vfs_t *vfs, struct statvfs *st) {

    int status = 0;
    volume_stat_t vstat;

    DEBUG_FUNCTION;

    if (statvfs(vfs->root, st) != 0) {
        status = -1;
        goto out;
    }

    st->f_bsize = ROZO_BSIZE;

    if (getxattr(vfs->root, VFS_BLOCKS_XATTR_KEY, &(st->f_blocks), sizeof (uint64_t)) == -1) {
        status = -1;
        goto out;
    }

    if (volume_stat(vfs->volume, &vstat) != 0) {
        status = -1;
        goto out;
    }

    st->f_bfree = vstat.bfree;

    if (getxattr(vfs->root, VFS_FILES_XATTR_KEY, &(st->f_files), sizeof (uint64_t)) == -1) {
        status = -1;
        goto out;
    }

    st->f_namemax = ROZO_FILENAME_MAX;

out:
    return status;
}

int vfs_attr(vfs_t *vfs, const char *vpath, vattr_t *attr) {

    int status = 0;
    char path[PATH_MAX];
    uuid_t uuids[ROZO_SAFE];
    int i;

    DEBUG_FUNCTION;

    if (getxattr(vfs_map(vfs, vpath, path), VFS_MF_UUID_XATTR_KEY, &(attr->uuid), sizeof (uuid_t)) == -1) {
        status = -1;
        goto out;
    }

    if (getxattr(vfs_map(vfs, vpath, path), VFS_MF_MPSS_XATTR_KEY, uuids, ROZO_SAFE * sizeof (uuid_t)) == -1) {
        status = -1;
        goto out;
    }

    // XXX May be a pointer is ok (no strcpy)
    for (i = 0; i < ROZO_SAFE; i++) {
        //XXX volume_lookup may return NULL on not found.
        uuid_copy(attr->uuids[i], uuids[i]);
        strcpy(attr->hosts[i], volume_lookup(vfs->volume, uuids[i])->host);
    }

out:
    return status;
}

int vfs_stat(vfs_t *vfs, const char *vpath, struct stat *st) {

    int status = 0;
    char path[PATH_MAX];

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (lstat(path, st) != 0) {
        status = -1;
        goto out;
    }

    st->st_blksize = ROZO_BSIZE;

    if (S_ISREG(st->st_mode)) {
        uint64_t size;
        if (getxattr(path, VFS_MF_SIZE_XATTR_KEY, &size, sizeof (uint64_t)) == -1) {
            status = -1;
            goto out;
        }
        st->st_size = size;
        st->st_blocks = size * 512 / ROZO_BSIZE;
    }

out:
    return status;
}

int vfs_readlink(vfs_t *vfs, const char *vtarget, char *vlink) {

    int status = 0;
    ssize_t len;
    char target[PATH_MAX];
    char link[PATH_MAX];

    DEBUG_FUNCTION;

    if ((len = readlink(vfs_map(vfs, vtarget, target), link, PATH_MAX)) == -1) {
        status = -1;
        goto out;
    }
    link[len] = '\0';

    vfs_unmap(vfs, link, vlink);

out:
    return status;
}

int vfs_mknod(vfs_t *vfs, const char *vpath, mode_t mode) {

    int status = 0, i;
    char path[PATH_MAX];
    uint64_t zero = 0;
    uint64_t files;
    uuid_t uuid;
    uuid_t mss[ROZO_SAFE];
    volume_ms_t volume_mss[ROZO_SAFE];

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (mknod(path, mode, 0) != 0) {
        status = -1;
        goto out;
    }

    uuid_generate(uuid);

    if (volume_distribute(vfs->volume, volume_mss) != 0) {
        status = -1;
        goto out;
    }

    for (i = 0; i < ROZO_SAFE; i++)
        uuid_copy(mss[i], volume_mss[i].uuid);

    if (setxattr(path, VFS_MF_SIZE_XATTR_KEY, &zero, sizeof (uint64_t), XATTR_CREATE) == -1) {
        status = -1;
        goto out;
    }

    if (setxattr(path, VFS_MF_UUID_XATTR_KEY, uuid, sizeof (uuid_t), XATTR_CREATE) == -1) {
        status = -1;
        goto out;
    }

    if (setxattr(path, VFS_MF_MPSS_XATTR_KEY, mss, ROZO_SAFE * sizeof (uuid_t), XATTR_CREATE) == -1) {
        status = -1;
        goto out;
    }

    if (getxattr(vfs->root, VFS_FILES_XATTR_KEY, &files, sizeof (uint64_t)) == -1) {
        status = -1;
        goto out;
    }
    files++;
    if (setxattr(vfs->root, VFS_FILES_XATTR_KEY, &files, sizeof (uint64_t), XATTR_REPLACE) == -1) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int vfs_mkdir(vfs_t *vfs, const char *vpath, mode_t mode) {

    char path[PATH_MAX];

    DEBUG_FUNCTION;

    return mkdir(vfs_map(vfs, vpath, path), mode);
}

int vfs_unlink(vfs_t *vfs, const char *vpath) {

    int status = 0, i;
    char path[PATH_MAX];
    struct stat st;
    uuid_t uuids_ms[ROZO_SAFE];
    uuid_t uuid_file;

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (lstat(path, &st) == -1) {
        status = -1;
        goto out;
    }

    if (S_ISREG(st.st_mode)) {

        if (getxattr(path, VFS_MF_UUID_XATTR_KEY, &(uuid_file), sizeof (uuid_t)) == -1) {
            status = -1;
            goto out;
        }

        if (getxattr(path, VFS_MF_MPSS_XATTR_KEY, uuids_ms, ROZO_SAFE * sizeof (uuid_t)) == -1) {
            status = -1;
            goto out;
        }
    }

    // XXX do we really need to call vfs_map again ?
    if (unlink(vfs_map(vfs, vpath, path)) == -1) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int vfs_rmdir(vfs_t *vfs, const char *vpath) {

    char path[PATH_MAX];

    DEBUG_FUNCTION;

    return rmdir(vfs_map(vfs, vpath, path));
}

int vfs_symlink(vfs_t *vfs, const char *vtarget, const char *vlink) {

    DEBUG_FUNCTION;

    char target[PATH_MAX];
    char link[PATH_MAX];
    return symlink(vfs_map(vfs, vtarget, target), vfs_map(vfs, vlink, link));
}

int vfs_rename(vfs_t *vfs, const char *vfrom, const char *vto) {

    char from[PATH_MAX];
    char to[PATH_MAX];

    DEBUG_FUNCTION;

    return rename(vfs_map(vfs, vfrom, from), vfs_map(vfs, vto, to));
}

int vfs_chmod(vfs_t *vfs, const char *vpath, mode_t mode) {

    char path[PATH_MAX];

    DEBUG_FUNCTION;

    return chmod(vfs_map(vfs, vpath, path), mode);
}

int vfs_truncate(vfs_t *vfs, const char *vpath, uint64_t offset) {

    int status = 0;
    char path[PATH_MAX];
    uint64_t size;

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (getxattr(path, VFS_MF_SIZE_XATTR_KEY, &size, sizeof (uint64_t)) == -1) {
        status = -1;
        goto out;
    }

    if (size < offset) {
        if (setxattr(path, VFS_MF_SIZE_XATTR_KEY, &offset, sizeof (uint64_t), XATTR_REPLACE) != 0) {
            status = -1;
            goto out;
        }
    }

out:
    return status;
}

// In the case of nmbs block having same distribution we send back nmbs * sizeof(distribution) bytes
// could be optimize (the wires stand point)
int vfs_read_block(vfs_t *vfs, const char *vpath, uint64_t mb, uint32_t nmbs, distribution_t *distribution) {

    int status = 0;
    char path[PATH_MAX];
    int fd;

    DEBUG_FUNCTION;

    if ((fd = open(vfs_map(vfs, vpath, path), O_RDONLY)) == -1) {
        severe("vfs_read_block failed: open file %s failed: %s", vfs_map(vfs, vpath, path), strerror(errno));
        status = -1;
        goto out;
    }

    if (pread(fd, distribution, nmbs * sizeof(distribution_t), 
                mb * sizeof(distribution_t)) != nmbs * sizeof(distribution_t)) {
        severe("vfs_read_block failed: pread in file %s failed: %s", vfs_map(vfs, vpath, path), strerror(errno));
        status = -1;
        goto out;
    }

out:
    if (fd != -1) close(fd);
    return status;
}

int64_t vfs_read(vfs_t *vfs, const char *vpath, uint64_t off, uint32_t len) {

    int64_t read;
    uint64_t size;
    struct stat st;
    time_t now;
    struct timeval times[2];
    char path[PATH_MAX];

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (stat(path, &st) == -1) {
        read = -1;
        goto out;
    }

    if (time(&now) == -1) {
        read = -1;
        goto out;
    }

    if (getxattr(path, VFS_MF_SIZE_XATTR_KEY, &size, sizeof (uint64_t)) == -1) {
        read = -1;
        goto out;
    }

    // EOF
    if (off > size) {
        read = -1;
        errno = 0;
        goto out;
    }

    times[0].tv_sec = now;
    times[0].tv_usec = 0;
    times[1].tv_sec = st.st_mtime;
    times[1].tv_usec = 0;
    if (utimes(path, times) == -1) {
        read = -1;
        goto out;
    }

    read = off + len < size ? len : size - off;
out:
    return read;
}

// TODO : For now we assume a vfs_write block can only be made for nmbs block with the same distribution.
int vfs_write_block(vfs_t *vfs, const char *vpath, uint64_t mb, uint32_t nmbs, distribution_t distribution) {

    int status = 0, i, read_result;
    char path[PATH_MAX];
    int fd = -1;
    distribution_t old_distribution;
    uuid_t uuids_ms[ROZO_SAFE];
    uuid_t uuid_file;

    DEBUG_FUNCTION;

    if (getxattr(vfs_map(vfs, vpath, path), VFS_MF_UUID_XATTR_KEY, &(uuid_file), sizeof(uuid_t)) == -1) {
        status = -1;
        goto out;
    }

    if (getxattr(vfs_map(vfs, vpath, path), VFS_MF_MPSS_XATTR_KEY, uuids_ms, ROZO_SAFE * sizeof(uuid_t)) == -1) {
        status = -1;
        goto out;
    }

    if ((fd = open(vfs_map(vfs, vpath, path), O_RDWR)) == -1) {
        status = -1;
        goto out;
    }

    if (lseek(fd, mb * sizeof(distribution_t), SEEK_SET) < 0) {
        status = -1;
        goto out;
    }

    for (i = 0; i < nmbs; i++) {
        if (write(fd, &distribution, sizeof(distribution_t)) != sizeof(distribution_t)) {
            status = -1;
            goto out;
        }
    }

out:
    if (fd != -1) close(fd);
    return status;
}

int64_t vfs_write(vfs_t *vfs, const char *vpath, uint64_t off, uint32_t len) {

    int64_t written;
    uint64_t size;
    char path[PATH_MAX];

    DEBUG_FUNCTION;

    vfs_map(vfs, vpath, path);

    if (getxattr(path, VFS_MF_SIZE_XATTR_KEY, &size, sizeof (uint64_t)) == -1) {
        written = -1;
        goto out;
    }

    if (utimes(path, NULL) == -1) {
        written = -1;
        goto out;
    }

    // If we will change the size of this file
    if (off + len > size) {
        uint64_t blocks;
        uint64_t oldsize;

        oldsize = size;
        size = off + len;
        if (setxattr(path, VFS_MF_SIZE_XATTR_KEY, &size, sizeof (uint64_t), XATTR_REPLACE) != 0) {
            written = -1;
            goto out;
        }

        if (getxattr(vfs->root, VFS_BLOCKS_XATTR_KEY, &blocks, sizeof (uint64_t)) == -1) {
            written = -1;
            goto out;
        }

        blocks += ((size - oldsize) / ROZO_BSIZE);

        // Update the nb. of blocks in vfs
        if (setxattr(vfs->root, VFS_BLOCKS_XATTR_KEY, &blocks, sizeof (uint64_t), XATTR_REPLACE) != 0) {
            written = -1;
            goto out;
        }
    }

    written = len;

out:
    return written;
}

DIR * vfs_opendir(vfs_t *vfs, const char *vpath) {

    char path[PATH_MAX];

    DEBUG_FUNCTION;

    return opendir(vfs_map(vfs, vpath, path));
}
