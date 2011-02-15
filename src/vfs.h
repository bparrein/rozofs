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

#ifndef _VFS_H
#define _VFS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <uuid/uuid.h>

#include "rozo.h"
#include "distribution.h"

typedef struct vattr {
    uuid_t uuid;
    uuid_t uuids[ROZO_SAFE];
    char hosts[ROZO_SAFE][ROZO_HOSTNAME_MAX];
} vattr_t;

typedef struct vfs {
    char root[PATH_MAX];
    volume_t *volume;
} vfs_t;

int vfs_create(const char *root);

int vfs_uuid(const char *root, uuid_t uuid);

int vfs_statfs(vfs_t *vfs, struct statvfs *st);

int vfs_attr(vfs_t *vfs, const char *vpath, vattr_t *attr);

int vfs_stat(vfs_t *vfs, const char *vpath, struct stat *st);

int vfs_readlink(vfs_t *vfs, const char *vtarget, char *vlink);

int vfs_mknod(vfs_t *vfs, const char *vpath, mode_t mode);

int vfs_mkdir(vfs_t *vfs, const char *vpath, mode_t mode);

int vfs_unlink(vfs_t *vfs, const char *vpath);

int vfs_rmdir(vfs_t *vfs, const char *vpath);

int vfs_symlink(vfs_t *vfs, const char *vtarget, const char *vlink);

int vfs_rename(vfs_t *vfs, const char *vfrom, const char *vto);

int vfs_chmod(vfs_t *vfs, const char *vpath, mode_t mode);

int vfs_truncate(vfs_t *vfs, const char *vpath, uint64_t offset);

int64_t vfs_read(vfs_t *vfs, const char *vpath, uint64_t off, uint32_t len);

int vfs_read_block(vfs_t *vfs, const char *vpath, uint64_t mb, uint32_t nmbs, distribution_t *distribution);

int64_t vfs_write(vfs_t *vfs, const char *vpath, uint64_t off, uint32_t len);

int vfs_write_block(vfs_t *vfs, const char *vpath, uint64_t mb, uint32_t nmbs, distribution_t distribution);

DIR * vfs_opendir(vfs_t *vfs, const char *vpath);

#endif
