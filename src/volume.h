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

#ifndef _VOLUME_H
#define _VOLUME_H

#include <stdint.h>
#include <limits.h>
#include <uuid/uuid.h>

#include "rozo.h"
#include "list.h"

/* meta storage */
typedef struct volume_ms {
    uuid_t uuid;
    char host[ROZO_HOSTNAME_MAX];
    uint64_t capacity;
} volume_ms_t;

typedef struct volume_ms_entry {
    volume_ms_t ms;
    list_t list;
} volume_ms_entry_t;

typedef struct volume {
    list_t mss ;
    pthread_rwlock_t lock;
} volume_t;

/* volume stat */
typedef struct volume_stat {
    uint16_t bsize;
    uint64_t bfree;
} volume_stat_t;

int volume_initialize(volume_t *volume);

int volume_release(volume_t *volume);

volume_ms_t * volume_lookup(volume_t *volume, uuid_t uuid);

int volume_balance(volume_t *volume);

int volume_stat(volume_t *volume, volume_stat_t *volume_stat);

int volume_distribute(volume_t *volume, volume_ms_t mss[ROZO_SAFE]);

#endif

