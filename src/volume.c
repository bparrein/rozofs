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

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <uuid/uuid.h>
#include "rozofs.h"
#include "log.h"
#include "list.h"
#include "xmalloc.h"
#include "rozofs.h"
#include "volume.h"
#include <pthread.h>
#include "storageclt.h"

static int volume_storage_compare(const void *ms1, const void *ms2) {
    const volume_storage_t *pms1 = ms1;
    const volume_storage_t *pms2 = ms2;
    return pms2->stat.free - pms1->stat.free;
}

static int cluster_compare_capacity(list_t * l1, list_t * l2) {
    cluster_t *e1 = list_entry(l1, cluster_t, list);
    cluster_t *e2 = list_entry(l2, cluster_t, list);
    return e1->free < e2->free;
}

static void cluster_print(volume_storage_t * a, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        volume_storage_t *p = a + i;
        printf("sid: %d, host: %s, size: %" PRIu64 ", free: %" PRIu64 "\n",
               p->sid, p->host, p->stat.size, p->stat.free);
    }
}

int mstorage_initialize(volume_storage_t * st, uint16_t sid,
                        const char *hostname) {
    int status = -1;
    DEBUG_FUNCTION;

    //XXX Check that the SID is unique

    if (sid == 0) {
        errno = EINVAL;
        fprintf(stderr, "SID %u is invalid (SID must be greater than 0)\n",
                sid);
        fatal("SID %u is invalid (SID must be greater than 0)", sid);
        goto out;
    }

    st->sid = sid;
    strcpy(st->host, hostname);
    st->stat.free = 0;
    st->stat.size = 0;

    status = 0;
out:
    return status;
}

int volume_initialize() {
    int status = 0;
    DEBUG_FUNCTION;

    list_init(&volume.mcs);

    if ((errno = pthread_rwlock_init(&volume.lock, NULL)) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int volume_release() {
    int status = 0;
    list_t *p, *q;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    list_for_each_forward_safe(p, q, &volume.mcs) {
        cluster_t *entry = list_entry(p, cluster_t, list);
        list_remove(p);
        free(entry->ms);
        free(entry);
    }

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    if ((errno = pthread_rwlock_destroy(&volume.lock)) != 0) {
        status = -1;
        goto out;
    }

out:
    return status;
}

int volume_register(uint16_t cid, volume_storage_t * storages, uint16_t nb_ms) {

    DEBUG_FUNCTION;

    cluster_t *cluster = (cluster_t *) xmalloc(sizeof (cluster_t));

    cluster->cid = cid;
    cluster->free = 0;
    cluster->size = 0;
    cluster->ms = storages;
    cluster->nb_ms = nb_ms;

    list_push_back(&volume.mcs, &cluster->list);

    return 0;
}

char *lookup_volume_storage(sid_t sid, char *host) {
    list_t *iterator;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_rdlock(&volume.lock)) != 0)
        goto out;

    list_for_each_forward(iterator, &volume.mcs) {
        cluster_t *entry = list_entry(iterator, cluster_t, list);
        volume_storage_t *it = entry->ms;

        while (it != entry->ms + entry->nb_ms) {
            if (sid == it->sid) {
                strcpy(host, it->host);
                break;
            }
            it++;
        }
    }

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0)
        goto out;

out:
    return host;
}

int volume_balance() {
    int status = -1;
    list_t *iterator;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0)
        goto out;

    list_for_each_forward(iterator, &volume.mcs) {
        cluster_t *entry = list_entry(iterator, cluster_t, list);
        volume_storage_t *it = entry->ms;
        entry->free = 0;
        entry->size = 0;
        while (it != entry->ms + entry->nb_ms) {
            storageclt_t sclt;
            if (storageclt_initialize(&sclt, it->host, it->sid) != 0) {
                warning("failed to join: %s,  %s", it->host, strerror(errno));
                it->stat.free = 0;
                it->stat.size = 0;
            } else {
                if (storageclt_stat(&sclt, &it->stat) != 0) {
                    warning("failed to stat: %s", it->host);
                    it->stat.free = 0;
                    it->stat.size = 0;
                }
            }
            storageclt_release(&sclt);
            entry->free += it->stat.free;
            entry->size += it->stat.size;
            it++;
        }

        qsort(entry->ms, entry->nb_ms, sizeof (volume_storage_t),
              volume_storage_compare);

    }

    list_sort(&volume.mcs, cluster_compare_capacity);

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0)
        goto out;

    status = 0;
out:
    return status;
}

// what if a cluster is < rozofs safe

static int cluster_distribute(cluster_t * cluster, uint16_t * sids) {
    int status = -1;
    int ms_found = 0;
    int i;
    DEBUG_FUNCTION;

    for (i = 0; i < cluster->nb_ms; i++) {
        volume_storage_t *p = (cluster->ms) + i;
        if (p->stat.free != 0)
            sids[ms_found++] = p->sid;
        if (ms_found == rozofs_safe) {
            status = 0;
            break;
        }
    }
    return status;
}

int volume_distribute(uint16_t * cid, uint16_t * sids) {
    int status = -1;
    int cluster_found = 0;
    DEBUG_FUNCTION;

    if (list_empty(&volume.mcs)) {
        errno = EINVAL;
        goto out;
    }

    if ((errno = pthread_rwlock_rdlock(&volume.lock)) != 0)
        status = -1;

    list_t *iterator;

    list_for_each_forward(iterator, &volume.mcs) {
        cluster_t *entry = list_entry(iterator, cluster_t, list);
        if (cluster_distribute(entry, sids) == 0) {
            cluster_found = 1;
            *cid = entry->cid;
            break;
        }
    }

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0)
        goto out;

    if (cluster_found == 0) {
        errno = ENOSPC;
        goto out;
    }
    status = 0;

out:
    return status;
}

// XXX : locks ?

void volume_stat(volume_stat_t * stat) {
    list_t *iterator;
    DEBUG_FUNCTION;

    stat->bsize = ROZOFS_BSIZE;
    stat->bfree = 0;

    list_for_each_forward(iterator, &volume.mcs) {
        stat->bfree +=
            list_entry(iterator, cluster_t, list)->free / ROZOFS_BSIZE;
    }
    stat->bfree =
        (long double) stat->bfree / ((double) rozofs_forward /
                                     (double) rozofs_inverse);
}

int volume_print() {
    int status = -1;
    list_t *p;
    uint16_t nb_clusters;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    nb_clusters = (uint16_t) list_size(&volume.mcs);
    if (printf("Volume:  Nb. of clusters: %hu\n", nb_clusters) < 0)
        goto out;

    list_for_each_forward(p, &volume.mcs) {
        cluster_t *cluster = list_entry(p, cluster_t, list);
        if (printf
            ("cluster %d, nb. of storages :%d, size: %" PRIu64 ", free: %"
             PRIu64 "\n", cluster->cid, cluster->nb_ms, cluster->size,
             cluster->free) < 0)
            goto out;
        cluster_print(cluster->ms, cluster->nb_ms);
    }

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    status = 0;
out:
    return status;
}

uint16_t volume_size() {
    list_t *p;
    uint16_t nb_storages = 0;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0) {
        nb_storages = 0;
        goto out;
    }

    list_for_each_forward(p, &volume.mcs) {
        cluster_t *cluster = list_entry(p, cluster_t, list);
        nb_storages += cluster->nb_ms;
    }

    if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0) {
        nb_storages = 0;
        goto out;
    }

out:
    return nb_storages;
}
