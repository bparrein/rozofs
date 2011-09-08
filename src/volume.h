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

#ifndef _VOLUME_H
#define _VOLUME_H

#include <stdint.h>
#include <limits.h>
#include "rozofs.h"
#include "list.h"

typedef struct volume_stat {
    uint16_t bsize;
    uint64_t bfree;
} volume_stat_t;

typedef struct volume_storage {
    uint16_t sid;
    char host[ROZOFS_HOSTNAME_MAX];
    sstat_t stat;
} volume_storage_t;

typedef struct cluster {
    uint16_t cid;
    volume_storage_t *ms;
    uint16_t nb_ms;
    uint64_t size;
    uint64_t free;
    list_t list;
} cluster_t;

typedef struct volume {
    list_t mcs;
    pthread_rwlock_t lock;
} volume_t;

volume_t volume;

int volume_initialize();

int volume_release();

int mstorage_initialize(volume_storage_t * st, uint16_t sid,
                        const char *hostname);

int volume_register(uint16_t cid, volume_storage_t * storages,
                    uint16_t ms_nb);

int volume_balance();

volume_storage_t *lookup_volume_storage(sid_t sid);

int volume_distribute(uint16_t * cid, uint16_t * sids);

void volume_stat(volume_stat_t * volume_stat);

int volume_print();

uint16_t volume_size();

#endif
