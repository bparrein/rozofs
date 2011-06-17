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

enum command {
    HELP,
    CREATE,
    START,
    STOP,
    RELOAD
};

typedef struct export_entry {
    export_t export;
    list_t list;
} export_entry_t;

long int layout;

static char exportd_config_file[PATH_MAX] = EXPORTD_DEFAULT_CONFIG;

static int exportd_command_flag = -1;

static pthread_rwlock_t exportd_lock;

static list_t exports;

static pthread_t exportd_thread;

extern void export_program_1(struct svc_req *rqstp, SVCXPRT * ctl_svc);

static SVCXPRT *exportd_svc = NULL;

static void *balance_volume(void *v) {
    struct timespec ts = { 2, 0 };

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    for (;;) {
        if (volume_balance() != 0) {
            severe("can't balance: %s", strerror(errno));
        }
        nanosleep(&ts, NULL);
    }
    return 0;
}

static int configure() {

    int status = -1, i, j;
    struct config_t config;
    struct config_setting_t *volume_settings = NULL;
    struct config_setting_t *export_settings = NULL;

    DEBUG_FUNCTION;

    config_init(&config);
    if (config_read_file(&config, exportd_config_file) == CONFIG_FALSE) {
        fatal("Can't read configuration file: %s at line: %d",
              config_error_text(&config), config_error_line(&config));
        errno = EIO;
        goto out;
    }
    // Get the layout settings
    if (!config_lookup_int(&config, "layout", &layout)) {
        fatal("%s", config_error_text(&config));
        errno = EIO;
        goto out;
    }
    // Get the volume settings
    if ((volume_settings = config_lookup(&config, "volume")) == NULL) {
        errno = ENOKEY;
        severe("can't find volume.");
        status = -1;
        goto out;
    }

    for (i = 0; i < config_setting_length(volume_settings); i++) {
        struct config_setting_t *cluster_settings = NULL;
        if ((cluster_settings =
             config_setting_get_elem(volume_settings, i)) == NULL) {
            errno = EIO;        //XXX
            severe("can't get setting element: %s.",
                   config_error_text(&config));
            status = -1;
            goto out;
        }

        volume_storage_t *storage = (volume_storage_t *)
            xmalloc(config_setting_length(cluster_settings) *
                    sizeof (volume_storage_t));

        for (j = 0; j < config_setting_length(cluster_settings); j++) {
            struct config_setting_t *mstorage_settings = NULL;
            uint16_t sid;
            const char *host;

            if ((mstorage_settings =
                 config_setting_get_elem(cluster_settings, j)) == NULL) {
                errno = EIO;    //XXX
                severe("can't get setting element: %s.",
                       config_error_text(&config));
                status = -1;
                goto out;
            }

            if (config_setting_lookup_int
                (mstorage_settings, "sid",
                 (long int *) &sid) == CONFIG_FALSE) {
                errno = ENOKEY;
                severe("can't find sid.");
                status = -1;
                goto out;
            }

            if (config_setting_lookup_string(mstorage_settings, "host", &host)
                == CONFIG_FALSE) {
                errno = ENOKEY;
                severe("can't find host.");
                status = -1;
                goto out;
            }

            if (mstorage_initialize(storage + j, sid, host) != 0) {
                fprintf(stderr, "Can't add storage: %s\n", strerror(errno));
                status = -1;
                goto out;
            }
        }

        cluster_t *cluster = (cluster_t *) xmalloc(sizeof (cluster_t));

        cluster->cid = i + 1;
        cluster->free = 0;
        cluster->size = 0;
        cluster->ms = storage;
        cluster->nb_ms = config_setting_length(cluster_settings);

        list_push_back(&volume.mcs, &cluster->list);
    }

    // Get the exports settings
    if ((export_settings = config_lookup(&config, "exports")) == NULL) {
        errno = ENOKEY;
        severe("can't find exports.");
        status = -1;
        goto out;
    }

    for (i = 0; i < config_setting_length(export_settings); i++) {
        struct config_setting_t *mfs_setting;
        export_entry_t *export_entry =
            (export_entry_t *) xmalloc(sizeof (export_entry_t));
        const char *root;

        if ((mfs_setting =
             config_setting_get_elem(export_settings, i)) == NULL) {
            errno = EIO;        //XXX
            severe("can't get setting element: %s.",
                   config_error_text(&config));
            status = -1;
            goto out;
        }

        if (config_setting_lookup_string(mfs_setting, "root", &root) ==
            CONFIG_FALSE) {
            errno = ENOKEY;
            severe("can't find root.");
            status = -1;
            goto out;
        }

        if (export_initialize(&export_entry->export, i, root) != 0) {
            severe("can't initialize export.");
            status = -1;
            goto out;
        }

        list_push_back(&exports, &export_entry->list);
    }
    status = 0;

out:
    config_destroy(&config);
    return status;
}

eid_t *exportd_lookup_id(ep_path_t path) {
    list_t *iterator;
    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exports) {
        export_entry_t *entry = list_entry(iterator, export_entry_t, list);
        if (strcmp(entry->export.root, path) == 0)
            return &entry->export.eid;
    }
    errno = EINVAL;
    return NULL;
}

export_t *exportd_lookup_export(eid_t eid) {
    list_t *iterator;
    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exports) {
        export_entry_t *entry = list_entry(iterator, export_entry_t, list);
        if (eid == entry->export.eid)
            return &entry->export;
    }
    warning("export not found.");
    errno = EINVAL;
    return NULL;
}

static int exportd_initialize() {
    //int fd;
    int status;
    DEBUG_FUNCTION;

    /*
       if ((fd = open(exportd_config_file, O_RDWR)) == -1) {
       fprintf(stderr, "exportd failed: configuration file %s: %s\n",
       exportd_config_file, strerror(errno));
       status = -1;
       goto out;
       }
       close(fd);
     */

    // initialize volume
    if (volume_initialize() != 0) {
        fprintf(stderr, "exportd failed: can't initialize volume: %s\n",
                strerror(errno));
        status = -1;
        goto out;
    }
    // initialize list of exports path
    list_init(&exports);

    if (pthread_rwlock_init(&exportd_lock, NULL) != 0) {
        fprintf(stderr, "exportd failed: can't initialize exportd lock %s\n",
                strerror(errno));
        status = -1;
        goto out;
    }
    // Configure volume
    if (configure() != 0) {
        fprintf(stderr, "Can't configure volume %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    status = 0;
out:
    return status;
}

static void on_start() {
    int sock;
    int one = 1;
    DEBUG_FUNCTION;

    if (rozo_initialize(layout) != 0) {
        fatal("can't initialise rozo %s", strerror(errno));
        return;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *) one, sizeof (one));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) one, sizeof (one));
    // XXX Buffers sizes hard coded
    exportd_svc =
        svctcp_create(sock, ROZO_RPC_BUFFER_SIZE, ROZO_RPC_BUFFER_SIZE);
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

    if (pthread_create(&exportd_thread, NULL, balance_volume, NULL) != 0) {
        fatal("can't create balancing thread %s", strerror(errno));
        return;
    }

    info("running.");
    svc_run();
}

static void on_stop() {
    list_t *p, *q;
    DEBUG_FUNCTION;

    svc_exit();

    svc_unregister(EXPORT_PROGRAM, EXPORT_VERSION);
    pmap_unset(EXPORT_PROGRAM, EXPORT_VERSION);
    if (exportd_svc) {
        svc_destroy(exportd_svc);
        exportd_svc = NULL;
    }

    pthread_cancel(exportd_thread);

    list_for_each_forward_safe(p, q, &exports) {
        export_entry_t *entry = list_entry(p, export_entry_t, list);
        export_release(&entry->export);
        list_remove(p);
        free(entry);
    }

    pthread_rwlock_destroy(&exportd_lock);

    volume_release();
    rozo_release();

    info("stopped.");
}

static void on_usr1() {
    DEBUG_FUNCTION;

    // Configure volume
    if (configure() != 0) {
        fatal("Can't configure volume %s\n", strerror(errno));
    }

}

static void usage() {
    printf("Rozo export daemon - %s\n", VERSION);
    printf
        ("Usage: exportd {--help | -h } | {--create path} | {[{--config | -c} file] --start | --stop | --reload}\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf
        ("\t-c, --config\tconfiguration file to use (default *install prefix*/etc/rozo/export.conf).\n");
    printf("\t--create [path]\tcreate a new export environment.\n");
    printf("\t--start\t\tstart the daemon.\n");
    printf("\t--stop\t\tstop the daemon.\n");
    printf
        ("\t--reload\treload the configuration file (the one used at start time).\n");
    exit(EXIT_FAILURE);
};

int main(int argc, char *argv[]) {
    int c;
    char root[PATH_MAX];
    static struct option long_options[] = {
        {"help", no_argument, &exportd_command_flag, HELP},
        {"create", required_argument, &exportd_command_flag, CREATE},
        {"start", no_argument, &exportd_command_flag, START},
        {"stop", no_argument, &exportd_command_flag, STOP},
        {"reload", no_argument, &exportd_command_flag, RELOAD},
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
            exportd_command_flag = HELP;
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

    switch (exportd_command_flag) {
    case HELP:
        usage();
        break;
    case CREATE:
        if (export_create(root) != 0) {
            fprintf(stderr, "exportd failed: export path: %s: %s\n", root,
                    strerror(errno));
        }
        break;
    case START:
        if (exportd_initialize() != 0) {
            exit(EXIT_FAILURE);
        }
        openlog("exportd", LOG_PID, LOG_DAEMON);
        daemon_start(EXPORTD_PID_FILE, on_start, on_stop, on_usr1);
        break;
    case STOP:
        daemon_stop(EXPORTD_PID_FILE);
        break;
    case RELOAD:
        daemon_usr1(EXPORTD_PID_FILE);
        break;
    default:
        usage();
    }

    exit(0);
}
