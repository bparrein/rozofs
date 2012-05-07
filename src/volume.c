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

    // The server online takes priority to the server offline
    if ((!pms1->status && pms2->status) || (pms1->status && !pms2->status)) {

        return (pms2->status - pms1->status);
    }

    return pms2->stat.free - pms1->stat.free;
}

static int cluster_compare_capacity(list_t * l1, list_t * l2) {
    cluster_t *e1 = list_entry(l1, cluster_t, list);
    cluster_t *e2 = list_entry(l2, cluster_t, list);
    return e1->free < e2->free;
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
    st->status = 0;

    status = 0;
out:
    return status;
}

int volumes_list_initialize() {
    int status = -1;
    DEBUG_FUNCTION;

    list_init(&volumes_list.vol_list);

    volumes_list.version = 0;

    if ((errno = pthread_rwlock_init(&volumes_list.lock, NULL)) != 0) {
        goto out;
    }

    status = 0;
out:
    return status;
}

int volume_release() {
    int status = -1;
    list_t *p, *q;
    list_t *i, *j;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
        goto out;

    list_for_each_forward_safe(p, q, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(p, volume_t, list);

        list_for_each_forward_safe(i, j, &entry_vol->cluster_list) {
            cluster_t *entry = list_entry(p, cluster_t, list);
            list_remove(i);
            free(entry->ms);
            free(entry);
        }

        list_remove(p);
        free(entry_vol);
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
        goto out;

    if ((errno = pthread_rwlock_destroy(&volumes_list.lock)) != 0)
        goto out;

    status = 0;
out:
    return status;
}

char *lookup_volume_storage(sid_t sid, char *host) {
    list_t *iterator;
    list_t *iterator_2;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_rdlock(&volumes_list.lock)) != 0)
        goto out;

    list_for_each_forward(iterator, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(iterator, volume_t, list);

        list_for_each_forward(iterator_2, &entry_vol->cluster_list) {
            cluster_t *entry = list_entry(iterator_2, cluster_t, list);
            volume_storage_t *it = entry->ms;

            while (it != entry->ms + entry->nb_ms) {
                if (sid == it->sid) {
                    strcpy(host, it->host);
                    break;
                }
                it++;
            }
        }
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
        goto out;

out:
    return host;
}

int volume_balance() {
    int status = -1;
    list_t *iterator;
    list_t *iterator_2;
    int nb_clusters = 0;
    volume_storage_t **st_cpy = NULL;
    uint8_t *st_nb = NULL;
    uint8_t old_ver = 0;
    int i = 0;
    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_tryrdlock(&volumes_list.lock)) != 0)
        goto out;

    old_ver = volumes_list.version;

    // Get number of cluster

    list_for_each_forward(iterator, &volumes_list.vol_list) {
        volume_t *entry_vol = list_entry(iterator, volume_t, list);
        nb_clusters = nb_clusters + list_size(&entry_vol->cluster_list);
    }

    st_cpy = xcalloc(nb_clusters, sizeof (volume_storage_t *));
    st_nb = xcalloc(nb_clusters, sizeof (uint8_t));

    list_for_each_forward(iterator, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(iterator, volume_t, list);

        list_for_each_forward(iterator_2, &entry_vol->cluster_list) {

            cluster_t *entry = list_entry(iterator_2, cluster_t, list);

            st_cpy[i] = xmalloc(entry->nb_ms * sizeof (volume_storage_t));

            memcpy(st_cpy[i], entry->ms, entry->nb_ms * sizeof (volume_storage_t));

            st_nb[i] = entry->nb_ms;

            i++;
        }
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
        goto out;


    // Try to join each storage server
    for (i = 0; i < nb_clusters; i++) {

        volume_storage_t *vs = st_cpy[i];

        while (vs != st_cpy[i] + st_nb[i]) {

            storageclt_t sclt;
            strcpy(sclt.host, vs->host);
            sclt.sid = vs->sid;

            if (storageclt_initialize(&sclt) != 0) {
                warning("failed to join: %s,  %s", vs->host, strerror(errno));
                vs->status = 0;
            } else {
                if (storageclt_stat(&sclt, &vs->stat) != 0) {
                    warning("failed to stat: %s", vs->host);
                    vs->status = 0;
                } else {
                    vs->status = 1;
                }
            }
            storageclt_release(&sclt);
            vs++;
        }
    }

    // Copy updated list
    if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
        goto out;

    i = 0;

    // Check if the volume list has been modified since the copy
    if (old_ver == volumes_list.version) {

        // For each volume

        list_for_each_forward(iterator, &volumes_list.vol_list) {

            volume_t *entry_vol = list_entry(iterator, volume_t, list);

            // For each cluster

            list_for_each_forward(iterator_2, &entry_vol->cluster_list) {

                cluster_t *entry = list_entry(iterator_2, cluster_t, list);

                memcpy(entry->ms, st_cpy[i], entry->nb_ms * sizeof (volume_storage_t));
                volume_storage_t *it = entry->ms;
                entry->free = 0;
                entry->size = 0;

                while (it != entry->ms + entry->nb_ms) {

                    entry->free += it->stat.free;
                    entry->size += it->stat.size;
                    it++;
                }

                // Sort this list of storages
                qsort(entry->ms, entry->nb_ms, sizeof (volume_storage_t), volume_storage_compare);

                i++;
            }

            // Sort this list of clusters for one volume
            list_sort(&entry_vol->cluster_list, cluster_compare_capacity);
        }
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
        goto out;

    status = 0;
out:
    if (st_cpy) {
        for (i = 0; i < nb_clusters; i++)
            if (st_cpy[i])
                free(st_cpy[i]);
        free(st_cpy);
    }
    if (st_nb)
        free(st_nb);
    return status;
}

// what if a cluster is < rozofs safe

static int cluster_distribute(cluster_t * cluster, uint16_t * sids) {
    int status = -1;
    int ms_found = 0;
    int ms_ok = 0;
    int i = 0;
    DEBUG_FUNCTION;

    for (i = 0; i < cluster->nb_ms; i++) {
        volume_storage_t *p = (cluster->ms) + i;

        if (p->status != 0 || p->stat.free != 0)
            ms_ok++;

        sids[ms_found++] = p->sid;

        // When creating a file we must be sure to have rozofs_safe servers
        // and have at least rozofs_server available for writing
        if (ms_found == rozofs_safe && ms_ok >= rozofs_forward) {
            status = 0;
            break;
        }
    }
    return status;
}

int volume_distribute(uint16_t * cid, uint16_t * sids, uint16_t vid) {
    int status = -1;
    int cluster_found = 0;
    DEBUG_FUNCTION;

    if (list_empty(&volumes_list.vol_list)) {
        errno = EINVAL;
        goto out;
    }

    if ((errno = pthread_rwlock_rdlock(&volumes_list.lock)) != 0)
        status = -1;

    list_t *iterator;
    list_t *iterator_2;

    // For each volume

    list_for_each_forward(iterator, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(iterator, volume_t, list);

        warning("volume_distribute: vid search: %u, vid: %u", entry_vol->vid, vid);

        // Get volume with this vid
        if (entry_vol->vid == vid) {

            list_for_each_forward(iterator_2, &entry_vol->cluster_list) {

                cluster_t *entry = list_entry(iterator_2, cluster_t, list);

                if (cluster_distribute(entry, sids) == 0) {
                    cluster_found = 1;
                    *cid = entry->cid;
                    break;
                }
            }
            break;
        }
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
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

void volume_stat(volume_stat_t * stat, uint16_t vid) {
    list_t *p, *q;

    DEBUG_FUNCTION;

    stat->bsize = ROZOFS_BSIZE;
    stat->bfree = 0;

    list_for_each_forward(p, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(p, volume_t, list);

        // Get volume with this vid
        if (entry_vol->vid == vid) {

            list_for_each_forward(q, &entry_vol->cluster_list) {

                stat->bfree += list_entry(q, cluster_t, list)->free / ROZOFS_BSIZE;
            }
        }
    }

    stat->bfree = (long double) stat->bfree / ((double) rozofs_forward / (double) rozofs_inverse);
}

int volume_exist(vid_t vid) {
    list_t *p;
    int status = -1;

    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_rdlock(&volumes_list.lock)) != 0)
        goto out;

    list_for_each_forward(p, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(p, volume_t, list);

        if (entry_vol->vid == vid) {
            status = 0;
            break;
        }
    }

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
        goto out;

out:
    return status;
}

/*
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
 */

/*
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
 */

/*
static void cluster_print(volume_storage_t * a, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        volume_storage_t *p = a + i;
        printf("sid: %d, host: %s, size: %" PRIu64 ", free: %" PRIu64 "\n",
                p->sid, p->host, p->stat.size, p->stat.free);
    }
}
 */


/*
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
 */
