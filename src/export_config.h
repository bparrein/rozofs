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

#ifndef EXPORT_CONFIG_H
#define EXPORT_CONFIG_H

#include <limits.h>
#include <uuid/uuid.h>

#include "rozofs.h"
#include "list.h"

typedef struct export_config_ms {
    uuid_t uuid;
    char host[ROZOFS_HOSTNAME_MAX];
} export_config_ms_t;

typedef struct export_config_ms_entry {
    export_config_ms_t export_config_ms;
    list_t list;
} export_config_ms_entry_t;

typedef struct export_config_mfs {
    char root[PATH_MAX];
    char md5pass[ROZOFS_MD5PASS_SIZE];
} export_config_mfs_t;

typedef struct export_config_mfs_entry {
    export_config_mfs_t export_config_mfs;
    list_t list;
} export_config_mfs_entry_t;

typedef struct export_config {
    list_t mss;
    list_t mfss;
} export_config_t;

int export_config_initialize(export_config_t * export_config,
                             const char *path);

int export_config_release(export_config_t * export_config);

void export_config_print(export_config_t * export_config);

#endif
