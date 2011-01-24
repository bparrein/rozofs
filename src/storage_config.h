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

#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <limits.h>
#include <uuid/uuid.h>

#include "rozo.h"
#include "list.h"

typedef struct storage_config_ms {
    uuid_t uuid;
    char root[PATH_MAX];
} storage_config_ms_t;

typedef struct storage_config_ms_entry {
    storage_config_ms_t storage_config_ms;
    list_t list;
} storage_config_ms_entry_t;

typedef list_t storage_config_t;

int storage_config_initialize(storage_config_t *storage_config, const char* path);

int storage_config_release(storage_config_t *storage_config);

void storage_config_print(storage_config_t *storage_config);

#endif

