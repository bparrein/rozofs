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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <getopt.h>
#include <libconfig.h>
#include <limits.h>
#include <unistd.h>
#include <rpc/pmap_clnt.h>

#include "config.h"
#include "log.h"
#include "daemon.h"
#include "volume.h"
#include "eproto.h"
#include "export.h"
#include "xmalloc.h"

#define EXPORTD_PID_FILE "exportd.pid"

long int layout;

typedef struct export_entry {
    export_t export;
    list_t list;
} export_entry_t;

static pthread_rwlock_t exports_lock;
static list_t exports;

static pthread_t bal_vol_thread;
static pthread_t rm_bins_thread;

static char exportd_config_file[PATH_MAX] = EXPORTD_DEFAULT_CONFIG;

static SVCXPRT *exportd_svc = NULL;

extern void export_program_1(struct svc_req *rqstp, SVCXPRT * ctl_svc);

static void *balance_volume_thread(void *v) {
    struct timespec ts = {8, 0};

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    for (;;) {
        if (volume_balance() != 0) {
            severe("balance_volume_thread failed: %s", strerror(errno));
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}

int exports_remove_bins() {
    int status = -1;
    list_t *iterator;
    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exports) {
        export_entry_t *entry = list_entry(iterator, export_entry_t, list);

        if (export_rm_bins(&entry->export) != 0) {
            goto out;
        }
    }
    status = 0;
out:
    return status;
}

static void *remove_bins_thread(void *v) {
    struct timespec ts = {30, 0};

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    for (;;) {
        if (exports_remove_bins() != 0) {
            severe("remove_bins_thread failed: %s", strerror(errno));
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}

eid_t *exports_lookup_id(ep_path_t path) {
    list_t *iterator;
    char export_path[PATH_MAX];
    DEBUG_FUNCTION;

    if (!realpath(path, export_path)) {
        return NULL;
    }

    list_for_each_forward(iterator, &exports) {
        export_entry_t *entry = list_entry(iterator, export_entry_t, list);
        if (strcmp(entry->export.root, export_path) == 0)
            return &entry->export.eid;
    }
    errno = EINVAL;
    return NULL;
}

export_t *exports_lookup_export(eid_t eid) {
    list_t *iterator;
    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exports) {
        export_entry_t *entry = list_entry(iterator, export_entry_t, list);
        if (eid == entry->export.eid)
            return &entry->export;
    }

    errno = EINVAL;
    return NULL;
}

int exports_initialize() {
    int status = -1;
    list_init(&exports);

    DEBUG_FUNCTION;

    if (pthread_rwlock_init(&exports_lock, NULL) != 0) {
        goto out;
    }
    status = 0;
out:
    return status;
}

static int load_layout_conf(struct config_t *config) {
    int status = -1;

    DEBUG_FUNCTION;

    // Get the layout setting
    if (!config_lookup_int(config, "layout", &layout)) {
        errno = EIO;
        fprintf(stderr, "cant't fetche layout setting\n");
        goto out;
    }

    if (rozofs_initialize(layout) != 0) {
        fprintf(stderr, "can't initialise rozofs layout: %s\n",
                strerror(errno));
        goto out;
    }
    status = 0;
out:
    return status;
}

static int load_volumes_conf(struct config_t *config) {
    int status = -1, v, c, s;
    struct config_setting_t *volumes_set = NULL;

    DEBUG_FUNCTION;

    // Get settings for volumes (list of volumes)
    if ((volumes_set = config_lookup(config, "volumes")) == NULL) {
        errno = ENOKEY;
        fprintf(stderr, "can't locate the volumes settings in conf file\n");
        goto out;
    }

    // For each volume
    for (v = 0; v < config_setting_length(volumes_set); v++) {

        long int vid; // Volume identifier
        struct config_setting_t *vol_set = NULL; // Settings for one volume
        /* Settings of list of clusters for one volume */
        struct config_setting_t *clu_list_set = NULL;
        volume_t *volume = NULL;

        // Get settings for ONE volume
        if ((vol_set = config_setting_get_elem(volumes_set, v)) == NULL) {
            errno = EIO;
            fprintf(stderr, "cant't fetche volume at index %d\n", v);
            goto out;
        }

        // Lookup vid for this volume
        if (config_setting_lookup_int(vol_set, "vid", &vid) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up vid for volume (idx=%d)\n", v);
            goto out;
        }

        // If this VID already exists
        if (volume_exist(vid) == 0) {
            fprintf(stderr, "cant't add volume with vid: %lu already exists\n", vid);
            goto out;
        }

        // Memory allocation for this volume
        volume = (volume_t *) xmalloc(sizeof (volume_t));

        // Initialize list of cluter(s) for this volume
        list_init(&volume->cluster_list);

        // Put vid for this volume
        volume->vid = vid;

        // Get settings for clusters for this volume
        if ((clu_list_set = config_setting_get_member(vol_set, "cids")) == NULL) {
            errno = ENOKEY;
            fprintf(stderr, "can't fetch cids for volume (vid=%ld)\n", vid);
            goto out;
        }

        // For each cluster of this volume
        for (c = 0; c < config_setting_length(clu_list_set); c++) {

            long int cid; // Cluster identifier
            struct config_setting_t *stor_set;
            struct config_setting_t *clu_set;

            // Get settings for ONE cluster
            if ((clu_set = config_setting_get_elem(clu_list_set, c)) == NULL) {
                errno = EIO; //XXX
                fprintf(stderr, "can't fetch cluster (idx=%d) in volume (vid=%ld)\n", c, vid);
                goto out;
            }

            // Lookup cid for this cluster
            if (config_setting_lookup_int(clu_set, "cid", &cid) == CONFIG_FALSE) {
                errno = ENOKEY;
                fprintf(stderr, "cant't look up cid for cluster (idx=%d)\n", c);
                goto out;
            }

            // Check if cid is unique in the volume that we are currently adding
            if (cluster_exist_vol(volume, cid) == 0) {
                fprintf(stderr, "cant't add cluster with cid: %lu already exists\n", cid);
                goto out;
            }

            // Check if cid is unique in the other(s) volume(s)
            if (cluster_exist(cid) == 0) {
                fprintf(stderr, "cant't add cluster with cid: %lu already exists\n", cid);
                goto out;
            }

            // Get settings for sids for this cluster
            if ((stor_set = config_setting_get_member(clu_set, "sids")) == NULL) {
                errno = ENOKEY;
                fprintf(stderr, "can't fetch sids for cluster (cid=%ld)\n", cid);
                goto out;
            }

            // Check if nb. of storages is sufficiant for this layout
            if (config_setting_length(stor_set) < rozofs_safe) {
                fprintf(stderr, "cluster (cid=%ld) doesn't have a sufficient number of storages for this layout\n", cid);
                goto out;
            }

            // Allocation of memory for storages
            volume_storage_t *storage = (volume_storage_t *) xmalloc(config_setting_length(stor_set) * sizeof (volume_storage_t));

            for (s = 0; s < config_setting_length(stor_set); s++) {

                struct config_setting_t *mstor_set = NULL;
                long int sid;
                const char *host;

                // Get settings for ONE storage
                if ((mstor_set = config_setting_get_elem(stor_set, s)) == NULL) {
                    errno = EIO; //XXX
                    fprintf(stderr, "can't fetch storage (idx=%d) in cluster (idx=%d)\n", s, c);
                    goto out;
                }

                // Lookup sid for this storage
                if (config_setting_lookup_int(mstor_set, "sid", &sid) == CONFIG_FALSE) {
                    errno = ENOKEY;
                    fprintf(stderr, "can't look up SID for storage (idx=%d) in cluster (idx=%d)\n", s, c);
                    goto out;
                }

                // Check sid (must be greater than 0)
                if (sid == 0) {
                    errno = EINVAL;
                    fprintf(stderr, "SID %lu is invalid (SID must be greater than 0)\n", sid);
                    goto out;
                }

                // Check if sid is unique in the cluster that we are currently adding
                if (storage_exist_cluster(storage, config_setting_length(stor_set), sid) == 0) {
                    fprintf(stderr, "can't add storage with sid: %lu already exists\n", sid);
                    goto out;
                }

                // Check if sid is unique in the volume that we are currently adding
                if (storage_exist_volume(volume, sid) == 0) {
                    fprintf(stderr, "can't add storage with sid: %lu already exists\n", sid);
                    goto out;
                }

                // Check if sid is unique in the other(s) volume(s)
                if (storage_exist(sid) == 0) {
                    fprintf(stderr, "can't add storage with sid: %lu already exists\n", sid);
                    goto out;
                }

                // Lookup hostname for this storage
                if (config_setting_lookup_string(mstor_set, "host", &host) == CONFIG_FALSE) {
                    errno = ENOKEY;
                    fprintf(stderr, "can't look up host for storage (idx=%d) in cluster (idx=%d)\n", s, c);
                    goto out;
                }

                if (mstorage_initialize(storage + s, (uint16_t) sid, host) != 0) {
                    fprintf(stderr, "can't add storage (SID=%ld)\n", sid);
                    goto out;
                }
            }

            // Memory allocation for this cluster
            cluster_t *cluster = (cluster_t *) xmalloc(sizeof (cluster_t));

            cluster->cid = (uint16_t) cid;
            cluster->free = 0;
            cluster->size = 0;
            cluster->ms = storage;
            cluster->nb_ms = config_setting_length(stor_set);

            if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
                goto out;

            // Add this cluster to the list of this volume
            list_push_back(&volume->cluster_list, &cluster->list);

            if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
                goto out;

        } // End add cluster

        if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
            goto out;

        // Add this volume to the list of volumes
        list_push_back(&volumes_list.vol_list, &volume->list);

        if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
            goto out;

    } // End add volume

    // Change version nb. of volumes list
    volumes_list.version++;

    status = 0;
out:
    return status;
}

/*
 * convert string to number of block of ROZOFS_BSIZE 
 */
static int strquota_to_nbblocks(const char *str, uint64_t *blocks) {
    int status = -1;
    char *unit;
    uint64_t value;

    errno = 0;
    value = strtol(str, &unit, 10);
    if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN))
                   || (errno != 0 && value == 0)) {
        goto out;
    }

    // no digit, no quota
    if (unit == str) {
        *blocks = 0;
        status = 0;
        goto out;
    }

    switch(*unit) {
        case 'K': 
            *blocks = 1024 * value / ROZOFS_BSIZE;
            break;
        case 'M': 
            *blocks = 1024 * 1024 * value / ROZOFS_BSIZE;
            break;
        case 'G':
            *blocks = 1024 * 1024 * 1024 * value / ROZOFS_BSIZE;
            break;
        default : // no unit user set directly nb blocks
            *blocks = value;
            break;
    }

    status = 0;

out:
    return status;
}

static int load_exports_conf(struct config_t *config) {
    int status = -1, i;
    struct config_setting_t *export_set = NULL;

    DEBUG_FUNCTION;

    // Get the exports settings
    if ((export_set = config_lookup(config, "exports")) == NULL) {
        errno = ENOKEY;
        fprintf(stderr, "can't locate the exports settings in conf file\n");
        goto out;
    }

    // For each export
    for (i = 0; i < config_setting_length(export_set); i++) {

        struct config_setting_t *mfs_setting = NULL;
        export_entry_t *export_entry = (export_entry_t *) xmalloc(sizeof (export_entry_t));
        const char *root;
        const char *md5;
        uint32_t eid; // Export identifier
        const char *str;
        uint64_t quota;
        long int vid; // Volume identifier

        if ((mfs_setting = config_setting_get_elem(export_set, i)) == NULL) {
            errno = EIO; //XXX
            fprintf(stderr, "can't fetch export at index %d\n", i);
            goto out;
        }

        if (config_setting_lookup_int(mfs_setting, "eid", (long int *) &eid)
                == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "can't look up eid for export (idx=%d)\n", i);
            goto out;
        }

        if (exports_lookup_export(eid) != NULL) {
            fprintf(stderr, "can't add export with eid %u: already exists\n", eid);
            goto out;
        }

        if (config_setting_lookup_string(mfs_setting, "root", &root) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "can't look up root path for export (idx=%d)\n", i);
            goto out;
        }

        if (exports_lookup_id((ep_path_t) root) != NULL) {
            fprintf(stderr, "can't add export with path %s: already exists\n", 
                    root);
            continue;
        }
        
        if (config_setting_lookup_string(mfs_setting, "md5", &md5) ==
                CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "can't look up md5 for export (idx=%d)\n", i);
            goto out;
        }

        if (config_setting_lookup_string(mfs_setting, "quota", &str) ==
                CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "can't look up quota for export (idx=%d)\n", i);
            goto out;
        }

        if (strquota_to_nbblocks(str, &quota) != 0) {
            fprintf(stderr, "%s: can't convert to quota)\n", str);
            goto out;
        }

        // Check if this path is unique in exports
        if (exports_lookup_id((ep_path_t) root) != NULL) {
            fprintf(stderr,
                    "cant't add export with path %s: already exists\n", root);
            continue;
        }

        // Lookup volume identifier
        if (config_setting_lookup_int(mfs_setting, "vid", &vid) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "can't look up vid for export (idx=%d)\n", i);
            goto out;
        }

        // Check vid exist in the volume
        if (volume_exist(vid) != 0) {
            fprintf(stderr, "can't add export with eid: %u (vid: %lu not exists)\n", eid, vid);
            goto out;
        }

        // Initialize export
        if (export_initialize(&export_entry->export, eid, root, md5, vid) != 0) {
            fprintf(stderr, "can't initialize export with path %s: %s\n",
                    root, strerror(errno));
            goto out;
        }

        if ((errno = pthread_rwlock_wrlock(&exports_lock)) != 0)
            goto out;

        // Add this export to the list of exports
        list_push_back(&exports, &export_entry->list);

        if ((errno = pthread_rwlock_unlock(&exports_lock)) != 0)
            goto out;
    }
    status = 0;
out:
    return status;
}

static int reload_exports_conf(struct config_t *config) {
    int status = -1, i;
    struct config_setting_t *export_set = NULL;

    DEBUG_FUNCTION;

    // Get the exports settings
    if ((export_set = config_lookup(config, "exports")) == NULL) {
        errno = ENOKEY;
        severe("can't locate the exports settings in conf file");
        goto out;
    }

    // For each export
    for (i = 0; i < config_setting_length(export_set); i++) {

        struct config_setting_t *mfs_setting = NULL;
        export_entry_t *export_entry = (export_entry_t *) xmalloc(sizeof (export_entry_t));
        const char *root;
        const char *md5;
        uint32_t eid; // Export identifier
        long int vid; // Volume identifier

        if ((mfs_setting = config_setting_get_elem(export_set, i)) == NULL) {
            errno = EIO; //XXX
            severe("cant't fetch export at index %d", i);
            goto out;
        }

        if (config_setting_lookup_int(mfs_setting, "eid", (long int *) &eid)
                == CONFIG_FALSE) {
            errno = ENOKEY;
            severe("cant't look up eid for export (idx=%d)", i);
            goto out;
        }

        // Check eid exist in the volume (old config)
        // If this eid already exist, go to the next export
        if (exports_lookup_export(eid) != NULL) {
            continue;
        }

        if (config_setting_lookup_string(mfs_setting, "root", &root) ==
                CONFIG_FALSE) {
            errno = ENOKEY;
            severe("cant't look up root path for export (idx=%d)", i);
            goto out;
        }

        if (config_setting_lookup_string(mfs_setting, "md5", &md5) ==
                CONFIG_FALSE) {
            errno = ENOKEY;
            severe("cant't look md5 for export (idx=%d)", i);
            goto out;
        }

        // Check if this path is unique in exports
        if (exports_lookup_id((ep_path_t) root) != NULL) {
            severe("cant't add export with path %s: already exists\n", root);
            continue;
        }

        // Lookup volume identifier
        if (config_setting_lookup_int(mfs_setting, "vid", &vid) == CONFIG_FALSE) {
            errno = ENOKEY;
            severe("cant't look up vid for export (idx=%d)", i);
            goto out;
        }

        // Check vid exist in the volume
        if (volume_exist(vid) != 0) {
            severe("cant't add export with eid: %u (vid: %lu not exists)", eid, vid);
            goto out;
        }

        // Initialize export
        if (export_initialize(&export_entry->export, eid, root, md5, vid) != 0) {
            severe("can't initialize export with path %s: %s", root,
                    strerror(errno));
            goto out;
        }

        if ((errno = pthread_rwlock_wrlock(&exports_lock)) != 0)
            goto out;

        // Add this export to the list of exports
        list_push_back(&exports, &export_entry->list);

        if ((errno = pthread_rwlock_unlock(&exports_lock)) != 0)
            goto out;

        info("add export with eid: %u", eid);
    }
    status = 0;
out:
    return status;
}

static int load_conf_file() {
    int status = -1, fd;
    struct config_t config;

    DEBUG_FUNCTION;

    config_init(&config);

    if ((fd = open(exportd_config_file, O_RDWR)) == -1) {
        fprintf(stderr, "can't load config file %s: %s\n",
                exportd_config_file, strerror(errno));
        status = -1;
        goto out;
    }
    close(fd);

    if (config_read_file(&config, exportd_config_file) == CONFIG_FALSE) {
        errno = EIO;
        fprintf(stderr, "can't read config file: %s at line: %d\n",
                config_error_text(&config), config_error_line(&config));
        goto out;
    }

    if (load_layout_conf(&config) != 0) {
        goto out;
    }

    if (load_volumes_conf(&config) != 0) {
        goto out;
    }

    if (load_exports_conf(&config) != 0) {
        goto out;
    }

    status = 0;

out:
    config_destroy(&config);
    return status;
}

static int reload_volumes_conf(struct config_t *config) {
    int status = -1, v, c, s;
    struct config_setting_t *volumes_set = NULL;

    DEBUG_FUNCTION;

    // Get settings for volumes (list of volumes)
    if ((volumes_set = config_lookup(config, "volumes")) == NULL) {
        errno = ENOKEY;
        severe("can't locate the volumes settings in conf file");
        goto out;
    }

    // For each volume
    for (v = 0; v < config_setting_length(volumes_set); v++) {

        long int vid; // Volume identifier
        struct config_setting_t *vol_set = NULL; // Settings for one volume
        /* Settings of list of clusters for one volume */
        struct config_setting_t *clu_list_set = NULL;
        volume_t *volume = NULL;

        // Get settings for ONE volume
        if ((vol_set = config_setting_get_elem(volumes_set, v)) == NULL) {
            errno = EIO;
            severe("cant't fetche volume at index %d", v);
            goto out;
        }

        // Lookup vid for this volume
        if (config_setting_lookup_int(vol_set, "vid", &vid) == CONFIG_FALSE) {
            errno = ENOKEY;
            severe("cant't look up vid for volume (idx=%d)", v);
            goto out;
        }

        // If this VID is a new volume
        if (volume_exist(vid) != 0) {
            // Memory allocation for this volume
            volume = (volume_t *) xmalloc(sizeof (volume_t));
            // Initialize list of cluter(s) for this volume
            list_init(&volume->cluster_list);
            // Put vid for this volume
            volume->vid = vid;
        }

        // Get settings for clusters for this volume
        if ((clu_list_set = config_setting_get_member(vol_set, "cids")) == NULL) {
            errno = ENOKEY;
            severe("can't fetch cids for volume (vid=%ld)", vid);
            goto out;
        }

        // For each cluster of this volume
        for (c = 0; c < config_setting_length(clu_list_set); c++) {

            long int cid; // Cluster identifier
            struct config_setting_t *stor_set = NULL;
            struct config_setting_t *clu_set = NULL;

            // Get settings for ONE cluster
            if ((clu_set = config_setting_get_elem(clu_list_set, c)) == NULL) {
                errno = EIO; //XXX
                severe("cant't fetch cluster at index (idx=%d) in volume (vid=%ld)", c, vid);
                goto out;
            }

            // Lookup cid for this cluster
            if (config_setting_lookup_int(clu_set, "cid", &cid) == CONFIG_FALSE) {
                errno = ENOKEY;
                severe("cant't look up cid for cluster (idx=%d)", c);
                goto out;
            }

            if (volume != NULL) {
                // Check if cid is unique in the volume that we are currently adding
                if (cluster_exist_vol(volume, cid) == 0) {
                    severe("cant't add cluster with cid: %lu already exists", cid);
                    goto out;
                }
            }

            // Check if cid is unique (compare with the old config)
            // If this cid already exists, continue without adding this cluster
            if (cluster_exist(cid) == 0) {
                continue;
            }

            // Get settings for sids for this cluster
            if ((stor_set = config_setting_get_member(clu_set, "sids")) == NULL) {
                errno = ENOKEY;
                severe("can't fetch sids for cluster (cid=%ld)", cid);
                goto out;
            }

            // Check if nb. of storages is sufficiant for this layout
            if (config_setting_length(stor_set) < rozofs_safe) {
                severe("cluster (cid=%ld) doesn't have a sufficient number of storage for this layout", cid);
                goto out;
            }

            // Allocation of memory for storages
            volume_storage_t *storage = (volume_storage_t *) malloc(config_setting_length(stor_set) * sizeof (volume_storage_t));

            for (s = 0; s < config_setting_length(stor_set); s++) {

                struct config_setting_t *mstor_set = NULL;
                long int sid;
                const char *host;

                // Get settings for ONE storage
                if ((mstor_set = config_setting_get_elem(stor_set, s)) == NULL) {
                    errno = EIO; //XXX
                    severe("cant't fetch storage at index (idx=%d) in cluster (idx=%d)", s, c);
                    goto out;
                }

                // Lookup sid for this storage
                if (config_setting_lookup_int(mstor_set, "sid", &sid) == CONFIG_FALSE) {
                    errno = ENOKEY;
                    severe("cant't look up SID for storage (idx=%d) in cluster (idx=%d)", s, c);
                    goto out;
                }

                // Check sid (must be greater than 0)
                if (sid == 0) {
                    errno = EINVAL;
                    fatal("SID %lu is invalid (SID must be greater than 0)", sid);
                    goto out;
                }

                // Check if sid is unique in the cluster that we are currently adding
                if (storage_exist_cluster(storage, config_setting_length(stor_set), sid) == 0) {
                    severe("cant't add storage with sid: %lu already exists", sid);
                    goto out;
                }

                if (volume != NULL) {
                    // Check if sid is unique in the volume that we are currently adding
                    if (storage_exist_volume(volume, sid) == 0) {
                        severe("cant't add storage with sid: %lu already exists", sid);
                        goto out;
                    }
                }

                // Check if sid is unique in the other(s) volume(s)
                if (storage_exist(sid) == 0) {
                    severe("cant't add storage with sid: %lu already exists", sid);
                    goto out;
                }

                // Lookup hostname for this storage
                if (config_setting_lookup_string(mstor_set, "host", &host) == CONFIG_FALSE) {
                    errno = ENOKEY;
                    severe("cant't look up host for storage (idx=%d) in cluster (idx=%d)", s, c);
                    goto out;
                }

                if (mstorage_initialize(storage + s, (uint16_t) sid, host) != 0) {
                    severe("can't add storage (SID=%ld)", sid);
                    goto out;
                }
            }

            // Memory allocation for this cluster
            cluster_t *cluster = (cluster_t *) xmalloc(sizeof (cluster_t));

            cluster->cid = (uint16_t) cid;
            cluster->free = 0;
            cluster->size = 0;
            cluster->ms = storage;
            cluster->nb_ms = config_setting_length(stor_set);

            // 2 CASES

            if (volume != NULL) {
                // Add this cluster to the list of this volume
                if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
                    goto out;

                list_push_back(&volume->cluster_list, &cluster->list);

                if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
                    goto out;
            } else {
                add_cluster_to_volume(vid, cluster);
            }

            info("add cluster (cid=%lu) to volume with vid=%lu", cid, vid);

        } // End add cluster

        if (volume != NULL) {
            if ((errno = pthread_rwlock_wrlock(&volumes_list.lock)) != 0)
                goto out;

            // Add this volume to the list of volumes
            list_push_back(&volumes_list.vol_list, &volume->list);

            if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0)
                goto out;

            info("add volume with vid: %lu", vid);
        }

    } // end add volume

    // Change version nb. of volumes list
    volumes_list.version++;

    status = 0;
out:
    return status;
}

int exports_release() {
    int status = -1;
    list_t *p, *q;

    list_for_each_forward_safe(p, q, &exports) {
        export_entry_t *entry = list_entry(p, export_entry_t, list);
        export_release(&entry->export);
        list_remove(p);
        free(entry);
    }

    if ((errno = pthread_rwlock_destroy(&exports_lock)) != 0) {
        goto out;
    }
    status = 0;
out:
    return status;
}

static int exportd_initialize() {
    int status = -1;
    DEBUG_FUNCTION;

    // Initialize list of volume(s)
    if (volumes_list_initialize() != 0) {
        fprintf(stderr, "can't initialize volume: %s\n", strerror(errno));
        goto out;
    }
    // Initialize list of exports
    if (exports_initialize() != 0) {
        fprintf(stderr, "can't initialize exports: %s\n", strerror(errno));
        goto out;
    }
    // Load configuration
    if (load_conf_file() != 0) {
        fprintf(stderr, "can't load settings from config file\n");
        goto out;
    }

    status = 0;
out:
    return status;
}

static void exportd_release() {

    pthread_cancel(bal_vol_thread);
    pthread_cancel(rm_bins_thread);

    exports_release();
    volume_release();
    rozofs_release();
}

static void on_start() {
    int sock;
    int one = 1;
    DEBUG_FUNCTION;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // SET NONBLOCK
    int value = 1;
    int oldflags = fcntl(sock, F_GETFL, 0);
    /* If reading the flags failed, return error indication now. */
    if (oldflags < 0) {
        return;
    }
    /* Set just the flag we want to set. */
    if (value != 0) {
        oldflags |= O_NONBLOCK;
    } else {
        oldflags &= ~O_NONBLOCK;
    }
    /* Store modified flag word in the descriptor. */
    fcntl(sock, F_SETFL, oldflags);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (int));
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof (int));
    setsockopt(sock, SOL_TCP, TCP_DEFER_ACCEPT, (char *) &one, sizeof (int));

    // XXX Buffers sizes hard coded
    exportd_svc =
            svctcp_create(sock, ROZOFS_RPC_BUFFER_SIZE, ROZOFS_RPC_BUFFER_SIZE);
    if (exportd_svc == NULL) {
        fatal("can't create service %s", strerror(errno));
        return;
    }

    pmap_unset(EXPORT_PROGRAM, EXPORT_VERSION); // in case !

    if (!svc_register
            (exportd_svc, EXPORT_PROGRAM, EXPORT_VERSION, export_program_1,
            IPPROTO_TCP)) {
        fatal("can't register service %s", strerror(errno));
        return;
    }

    if (pthread_create(&bal_vol_thread, NULL, balance_volume_thread, NULL) !=
            0) {
        fatal("can't create balancing thread %s", strerror(errno));
        return;
    }

    if (pthread_create(&rm_bins_thread, NULL, remove_bins_thread, NULL) != 0) {
        fatal("can't create remove files thread %s", strerror(errno));
        return;
    }

    info("running.");
    svc_run();
}

static void on_stop() {
    DEBUG_FUNCTION;

    exportd_release();

    svc_exit();

    svc_unregister(EXPORT_PROGRAM, EXPORT_VERSION);
    pmap_unset(EXPORT_PROGRAM, EXPORT_VERSION);
    if (exportd_svc) {
        svc_destroy(exportd_svc);
        exportd_svc = NULL;
    }
    info("stopped.");
    closelog();
}

static void on_hup() {
    int fd = -1;
    int status = -1;
    struct config_t config;
    DEBUG_FUNCTION;

    config_init(&config);

    info("Reload RozoFS export daemon");

    if ((fd = open(exportd_config_file, O_RDWR)) == -1) {
        severe("Failed to reload config file %s: %s", exportd_config_file, strerror(errno));
        goto out;
    }
    close(fd);

    if (config_read_file(&config, exportd_config_file) == CONFIG_FALSE) {
        errno = EIO;
        severe("Failed to read config file: %s at line: %d", config_error_text(&config), config_error_line(&config));
        goto out;
    }

    if (reload_volumes_conf(&config) != 0) {
        severe("Failed to reload completely the volumes list from configuration file");
        goto out;
    }

    info("Reload volume(s) list : success");

    if (reload_exports_conf(&config) != 0) {
        severe("Failed to reload completely the exports list from configuration file");
        goto out;
    }

    info("Reload export(s) list : success");

    status = 0;
out:
    if (status == 0) {
        info("Reload config file : success");
    }
    config_destroy(&config);
}

static void usage() {
    printf("Rozofs export daemon - %s\n", VERSION);
    printf("Usage: exportd [OPTIONS]\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf("\t-c, --config\tconfiguration file to use (default: %s).\n",
            EXPORTD_DEFAULT_CONFIG);
};

int main(int argc, char *argv[]) {
    int c;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    while (1) {

        int option_index = 0;
        c = getopt_long(argc, argv, "hc:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {

            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'c':
                if (!realpath(optarg, exportd_config_file)) {
                    fprintf(stderr,
                            "exportd failed: configuration file: %s: %s\n",
                            optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                usage();
                exit(EXIT_SUCCESS);
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
                break;
        }
    }
    if (exportd_initialize() != 0) {
        fprintf(stderr, "exportd start failed\n");
        exit(EXIT_FAILURE);
    }
    openlog("exportd", LOG_PID, LOG_DAEMON);
    daemon_start(EXPORTD_PID_FILE, on_start, on_stop, on_hup);

    exit(0);
}
