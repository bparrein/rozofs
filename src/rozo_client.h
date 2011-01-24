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

#ifndef _ROZO_CLIENT_H
#define _ROZO_CLIENT_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <uuid/uuid.h>

#include "rpc_client.h"

typedef struct rozo_client {
    rpc_client_t export_client;
    uuid_t export_uuid;
} rozo_client_t;

int rozo_client_initialize(rozo_client_t *rozo_client, const char *host, char *export);

int rozo_client_release(rozo_client_t *rozo_client);

int rozo_client_statfs(rozo_client_t *rozo_client, struct statvfs *st);

int rozo_client_stat(rozo_client_t *rozo_client, const char *path, struct stat *st);

int rozo_client_readlink(rozo_client_t *rozo_client, const char *target, char *link);

int rozo_client_mknod(rozo_client_t *rozo_client, const char *path, mode_t mode);

int rozo_client_mkdir(rozo_client_t *rozo_client, const char *path, mode_t mode);

typedef struct rozo_dirent {
    char *name;
    struct rozo_dirent *next;
} rozo_dirent_t;

rozo_dirent_t * rozo_client_readdir(rozo_client_t *rozo_client, const char *path);

int rozo_client_release_dirent(rozo_dirent_t *dirent);

int rozo_client_unlink(rozo_client_t *rozo_client, const char *path);

int rozo_client_rmdir(rozo_client_t *rozo_client, const char *path);

int rozo_client_symlink(rozo_client_t *rozo_client, const char *target, const char *link);

int rozo_client_rename(rozo_client_t *rozo_client, const char *from, const char *to);

int rozo_client_chmod(rozo_client_t *rozo_client, const char *path, mode_t mode);

int rozo_client_truncate(rozo_client_t *rozo_client, const char *path, uint64_t offset);

typedef struct rozo_file rozo_file_t;

rozo_file_t * rozo_client_open(rozo_client_t *rozo_client, const char *path, mode_t mode);

int64_t rozo_client_read(rozo_client_t *rozo_client, rozo_file_t *file, uint64_t off, char *buf, uint32_t len);

int64_t rozo_client_write(rozo_client_t *rozo_client, rozo_file_t *file, uint64_t off, const char *buf, uint32_t len);

int rozo_client_close(rozo_file_t *file);

#endif
