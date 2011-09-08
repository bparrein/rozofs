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
    struct timespec ts = { 2, 0 };

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
    struct timespec ts = { 35, 0 };

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

    if (pthread_rwlock_init(&exports_lock, NULL) != 0) {
        goto out;
    }
    status = 0;
out:
    return status;
}

static int load_layout_conf(struct config_t *config) {
    int status = -1;

    // Get the layout setting
    if (!config_lookup_int(config, "layout", &layout)) {
        errno = EIO;
        fprintf(stderr, "cant't fetche layout setting\n");
        fatal("cant't fetche layout setting");
        goto out;
    }

    if (rozofs_initialize(layout) != 0) {
        fprintf(stderr, "can't initialise rozofs layout: %s\n",
                strerror(errno));
        fatal("can't initialise rozofs layout: %s", strerror(errno));
        goto out;
    }
    status = 0;
out:
    return status;
}

static int load_volume_conf(struct config_t *config) {
    int status = -1, i, j;
    struct config_setting_t *vol_set = NULL;

    // Get the volume settings
    if ((vol_set = config_lookup(config, "volume")) == NULL) {
        errno = ENOKEY;
        fprintf(stderr, "can't locate the volume settings in conf file\n");
        fatal("can't locate the volume settings in conf file");
        goto out;
    }
    // For each cluster
    for (i = 0; i < config_setting_length(vol_set); i++) {

        long int cid;
        struct config_setting_t *stor_set;
        struct config_setting_t *clu_set;

        if ((clu_set = config_setting_get_elem(vol_set, i)) == NULL) {
            errno = EIO;
            fprintf(stderr, "cant't fetche cluster at index %d\n", i);
            fatal("cant't fetche cluster at index %d", i);
            goto out;
        }

        if (config_setting_lookup_int(clu_set, "cid", &cid) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up cid for cluster (idx=%d)\n", i);
            fatal("cant't look up cid for cluster (idx=%d)", i);
            goto out;
        }

        if ((stor_set = config_setting_get_member(clu_set, "sids")) == NULL) {
            errno = ENOKEY;
            fprintf(stderr, "can't fetche sids for cluster (cid=%ld)\n", cid);
            fatal("can't fetche sids for cluster (cid=%ld)", cid);
            goto out;
        }

        volume_storage_t *storage =
            (volume_storage_t *) xmalloc(config_setting_length(stor_set) *
                                         sizeof (volume_storage_t));

        for (j = 0; j < config_setting_length(stor_set); j++) {

            struct config_setting_t *mstor_set = NULL;
            long int sid;
            const char *host;

            if ((mstor_set = config_setting_get_elem(stor_set, j)) == NULL) {
                errno = EIO;    //XXX
                fprintf(stderr,
                        "cant't fetche storage (idx=%d) in cluster (idx=%d)\n",
                        j, i);
                fatal
                    ("cant't fetche storage at index (idx=%d) in cluster (idx=%d)",
                     j, i);
                goto out;
            }

            if (config_setting_lookup_int(mstor_set, "sid", &sid) ==
                CONFIG_FALSE) {
                errno = ENOKEY;
                fprintf(stderr,
                        "cant't look up SID for storage (idx=%d) in cluster (idx=%d)\n",
                        j, i);
                fatal
                    ("cant't look up SID for storage (idx=%d) in cluster (idx=%d)",
                     j, i);
                goto out;
            }

            if (config_setting_lookup_string(mstor_set, "host", &host) ==
                CONFIG_FALSE) {
                errno = ENOKEY;
                fprintf(stderr,
                        "cant't look up host for storage (idx=%d) in cluster (idx=%d)\n",
                        j, i);
                fatal
                    ("cant't look up host for storage (idx=%d) in cluster (idx=%d)",
                     j, i);
                goto out;
            }

            if (mstorage_initialize(storage + j, (uint16_t) sid, host) != 0) {
                fprintf(stderr, "can't add storage (SID=%ld)\n", sid);
                fatal("can't add storage (SID=%ld)", sid);
                goto out;
            }
        }

        if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0)
            goto out;

        list_t *iterator;

        list_for_each_forward(iterator, &volume.mcs) {
            cluster_t *entry = list_entry(iterator, cluster_t, list);
            if (cid == entry->cid) {
                fprintf(stderr,
                        "cant't add cluster with cid %ld: already exists\n",
                        cid);
                info("cant't add cluster with cid %ld: already exists", cid);
                continue;
            }
        }

        if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0)
            goto out;


        cluster_t *cluster = (cluster_t *) xmalloc(sizeof (cluster_t));

        cluster->cid = (uint16_t) cid;
        cluster->free = 0;
        cluster->size = 0;
        cluster->ms = storage;
        cluster->nb_ms = config_setting_length(stor_set);

        if ((errno = pthread_rwlock_wrlock(&volume.lock)) != 0)
            goto out;

        list_push_back(&volume.mcs, &cluster->list);

        if ((errno = pthread_rwlock_unlock(&volume.lock)) != 0)
            goto out;
    }

    status = 0;
out:
    return status;
}

static int load_exports_conf(struct config_t *config) {
    int status = -1, i;
    struct config_setting_t *export_set = NULL;

    // Get the exports settings
    if ((export_set = config_lookup(config, "exports")) == NULL) {
        errno = ENOKEY;
        fprintf(stderr, "can't locate the exports settings in conf file\n");
        severe("can't locate the exports settings in conf file");
        goto out;
    }

    for (i = 0; i < config_setting_length(export_set); i++) {

        struct config_setting_t *mfs_setting;
        export_entry_t *export_entry =
            (export_entry_t *) xmalloc(sizeof (export_entry_t));
        const char *root;
        uint32_t eid;

        if ((mfs_setting = config_setting_get_elem(export_set, i)) == NULL) {
            errno = EIO;        //XXX
            fprintf(stderr, "cant't fetche export at index %d\n", i);
            severe("cant't fetche export at index %d", i);
            goto out;
        }

        if (config_setting_lookup_int(mfs_setting, "eid", (long int *) &eid)
            == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up eid for export (idx=%d)\n", i);
            fatal("cant't look up eid for export (idx=%d)", i);
            goto out;
        }

        if (exports_lookup_export(eid) != NULL) {
            fprintf(stderr, "cant't add export with eid %u: already exists\n",
                    eid);
            info("cant't add export with eid %u: already exists", eid);
            continue;
        }

        if (config_setting_lookup_string(mfs_setting, "root", &root) ==
            CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up root path for export (idx=%d)\n",
                    i);
            severe("cant't look up root path for export (idx=%d)", i);
            goto out;
        }

        if (exports_lookup_id((ep_path_t) root) != NULL) {
            fprintf(stderr,
                    "cant't add export with path %s: already exists\n", root);
            info("cant't add export with path %s: already exists\n", root);
            continue;
        }

        if (export_initialize(&export_entry->export, eid, root) != 0) {
            fprintf(stderr, "can't initialize export with path %s: %s\n",
                    root, strerror(errno));
            severe("can't initialize export with path %s: %s", root,
                   strerror(errno));
            goto out;
        }

        if ((errno = pthread_rwlock_wrlock(&exports_lock)) != 0)
            goto out;

        list_push_back(&exports, &export_entry->list);

        if ((errno = pthread_rwlock_unlock(&exports_lock)) != 0)
            goto out;

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
        fatal("can't load config file %s: %s", exportd_config_file,
              strerror(errno));
        status = -1;
        goto out;
    }
    close(fd);

    if (config_read_file(&config, exportd_config_file) == CONFIG_FALSE) {
        errno = EIO;
        fprintf(stderr, "can't read config file: %s at line: %d\n",
                config_error_text(&config), config_error_line(&config));
        fatal("can't read config file: %s at line: %d",
              config_error_text(&config), config_error_line(&config));
        goto out;
    }

    if (load_layout_conf(&config) != 0) {
        goto out;
    }

    if (load_volume_conf(&config) != 0) {
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

    // Initialize list of clusters
    if (volume_initialize() != 0) {
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

    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof (int));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (int));
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
}

static void on_hup() {
    int fd = -1;
    struct config_t config;
    DEBUG_FUNCTION;

    config_init(&config);

    if ((fd = open(exportd_config_file, O_RDWR)) == -1) {
        fprintf(stderr, "can't load config file %s: %s\n",
                exportd_config_file, strerror(errno));
        fatal("can't load config file %s: %s", exportd_config_file,
              strerror(errno));
        goto out;
    }
    close(fd);

    if (config_read_file(&config, exportd_config_file) == CONFIG_FALSE) {
        errno = EIO;
        fprintf(stderr, "can't read config file: %s at line: %d\n",
                config_error_text(&config), config_error_line(&config));
        fatal("can't read config file: %s at line: %d",
              config_error_text(&config), config_error_line(&config));
        goto out;
    }

    if (load_exports_conf(&config) != 0)
        goto out;

    if (load_volume_conf(&config) != 0)
        goto out;

out:
    config_destroy(&config);
}

static void usage() {
    printf("Rozofs export daemon - %s\n", VERSION);
    printf("Usage: exportd [OPTIONS]\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf
        ("\t-c, --config\tconfiguration file to use (default *install prefix*/etc/rozofs/export.conf).\n");
    printf("\t--create [path]\tcreate a new export environment.\n");
    printf
        ("\t--reload\treload the configuration file (the one used at start time).\n");
};

int main(int argc, char *argv[]) {
    int c;
    char root[PATH_MAX];
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
        case 0:
            if (strcmp(long_options[option_index].name, "create") == 0)
                if (!realpath(optarg, root)) {
                    fprintf(stderr, "exportd failed: export path: %s: %s\n",
                            optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            break;
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
            exit(EXIT_FAILURE);
        default:
            exit(EXIT_FAILURE);
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
