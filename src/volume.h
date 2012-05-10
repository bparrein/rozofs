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
    uint16_t sid; // Storage identifier
    char host[ROZOFS_HOSTNAME_MAX];
    uint8_t status;
    sstat_t stat;
} volume_storage_t;

typedef struct cluster {
    uint16_t cid; // Cluster identifier
    volume_storage_t *ms;
    uint16_t nb_ms;
    uint64_t size;
    uint64_t free;
    list_t list;
} cluster_t;

typedef struct volume {
    uint16_t vid; // Volume identifier
    list_t list;
    list_t cluster_list; // Cluster(s) list
    // pthread_rwlock_t lock;
} volume_t;

typedef struct volumes_list {
    list_t vol_list; // Volume(s) list
    uint8_t version;
    pthread_rwlock_t lock;
} volumes_list_t;

volumes_list_t volumes_list;

int volumes_list_initialize();

int volume_release();

int mstorage_initialize(volume_storage_t * st, uint16_t sid,
        const char *hostname);

int volume_register(uint16_t cid, volume_storage_t * storages,
        uint16_t ms_nb);

int volume_balance();

char *lookup_volume_storage(sid_t sid, char *host);

int volume_distribute(uint16_t * cid, uint16_t * sids, uint16_t vid);

void volume_stat(volume_stat_t * volume_stat, uint16_t vid);

int volume_print();

uint16_t volume_size();

int volume_exist(vid_t vid);

int cluster_exist(cid_t cid);

int cluster_exist_vol(volume_t * v, cid_t cid);

int storage_exist(sid_t sid);

int storage_exist_volume(volume_t * v, sid_t sid);

int storage_exist_cluster(volume_storage_t * vs, int nb, sid_t sid);

int add_cluster_to_volume(vid_t vid, cluster_t * cluster);
#endif
