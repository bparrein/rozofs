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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>

#include "log.h"
#include "storage_proto.h"
#include "volume.h"

static int volume_compare_capacity(list_t *l1, list_t *l2) {
    volume_ms_entry_t *e1 = list_entry(l1, volume_ms_entry_t, list);
    volume_ms_entry_t *e2 = list_entry(l2, volume_ms_entry_t, list);
    return e1->ms.capacity < e2->ms.capacity;
}

int volume_initialize(volume_t *volume) {

    int status = 0;

    DEBUG_FUNCTION;

    list_init(&volume->mss);
    if ((errno = pthread_rwlock_init(&volume->lock, NULL)) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int volume_release(volume_t *volume) {

    int status = 0;

    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_destroy(&volume->lock)) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

//XXX lock ?
volume_ms_t * volume_lookup(volume_t *volume, uuid_t uuid) {

    list_t *iterator;

    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &volume->mss) {
        volume_ms_entry_t *entry = list_entry(iterator, volume_ms_entry_t, list);
        if (uuid_compare(uuid, entry->ms.uuid) == 0) {
            return &entry->ms;
        }
    }

    errno = EINVAL;
    return NULL;
}

int volume_balance(volume_t *volume) {

    int status = 0;
    list_t *iterator;

    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &volume->mss) {
        volume_ms_entry_t *entry = list_entry(iterator, volume_ms_entry_t, list);
        CLIENT *client = clnt_create(entry->ms.host, STORAGE_PROGRAM, STORAGE_VERSION, "tcp");
        if (client == NULL) {
            entry->ms.capacity = 0;
        } else {
            storage_stat_response_t *response;
            storage_uuid_t arg;

            uuid_copy(arg, entry->ms.uuid);
            response = storageproc_stat_1(arg, client);
            if (response == NULL) {
                entry->ms.capacity = 0;
            } else {
                entry->ms.capacity = response->storage_stat_response_t_u.stat.bfree * ROZO_BSIZE /
                        response->storage_stat_response_t_u.stat.bsize;
            }
            clnt_destroy(client);
        }
    }

    if ((errno = pthread_rwlock_wrlock(&volume->lock)) != 0) {
        status = -1;
        goto out;
    }

    list_sort(&volume->mss, volume_compare_capacity);

    if ((errno = pthread_rwlock_unlock(&volume->lock)) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

// XXX : locks ?
int volume_stat(volume_t *volume, volume_stat_t *stat) {

    list_t *iterator;

    DEBUG_FUNCTION;

    stat->bsize = ROZO_BSIZE;
    stat->bfree = 0;

    list_for_each_forward(iterator, &volume->mss) {
        stat->bfree += list_entry(iterator, volume_ms_entry_t, list)->ms.capacity * 1024 / ROZO_BSIZE;
    }

    return 0;
}

int volume_distribute(volume_t *volume, volume_ms_t mss[ROZO_SAFE]) {

    int status = 0;
    int ms_found = 0;

    DEBUG_FUNCTION;

    if (list_empty(&volume->mss)) {
        errno = EINVAL;
        status = -1;
        goto out;
    }

    if ((errno = pthread_rwlock_rdlock(&volume->lock)) != 0) {
        status = -1;
        goto out;
    }

    ms_found = 0;
    do {
        list_t *iterator;
        list_for_each_forward(iterator, &volume->mss) {
            volume_ms_entry_t *entry = list_entry(iterator, volume_ms_entry_t, list);
            if (entry->ms.capacity != 0)
                memcpy(&mss[ms_found++], &list_entry(iterator, volume_ms_entry_t, list)->ms, sizeof(volume_ms_t));
            if (ms_found == ROZO_SAFE)
                break;
        }
    } while(ms_found > 0 && ms_found < ROZO_SAFE);

    if ((errno = pthread_rwlock_unlock(&volume->lock)) != 0) {
        status = -1;
        goto out;
    }

    if (ms_found == 0) {
        errno = ENOENT;
        status = -1;
    }

out:
    return status;
}

