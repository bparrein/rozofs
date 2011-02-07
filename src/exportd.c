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

#include "config.h"
#include "log.h"
#include "daemon.h"
#include "volume.h"
#include "vfs.h"
#include "export_proto.h"
#include "export_config.h"

#define EXPORTD_PID_FILE "exportd.pid"

enum command {
    HELP,
    CREATE,
    START,
    STOP,
    RELOAD
};

typedef struct exportd_vfs_entry {
    uuid_t uuid;
    vfs_t vfs;
    list_t list;
} exportd_vfs_entry_t;

static list_t exportd_vfss;

static volume_t exportd_volume;

static char exportd_config_file[PATH_MAX] = EXPORTD_DEFAULT_CONFIG;

static int exportd_command_flag = -1;

extern void export_program_1(struct svc_req *rqstp, SVCXPRT *ctl_svc);

static SVCXPRT *exportd_svc = NULL;

static pthread_rwlock_t exportd_lock;

static pthread_t exportd_thread;

static int load_config(export_config_t *config) {

    int status;
    list_t *p, *q;

    DEBUG_FUNCTION;

    if ((errno = pthread_rwlock_wrlock(&exportd_lock)) != 0) {
        status = -1;
        goto out;
    }

    // clear and load volume
    if ((errno = pthread_rwlock_wrlock(&exportd_volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    list_for_each_forward_safe(p, q, &exportd_volume.mss) {
        volume_ms_entry_t *entry = list_entry(p, volume_ms_entry_t, list);
        list_remove(p);
        free(entry);
    }

    list_for_each_forward(p, &config->mss) {
        export_config_ms_entry_t *export_config_ms_entry;
        volume_ms_entry_t *volume_ms_entry;

        if ((volume_ms_entry = malloc(sizeof (volume_ms_entry_t))) == NULL) {
            status = -1;
            goto out;
        }

        export_config_ms_entry = list_entry(p, export_config_ms_entry_t, list);

        uuid_copy(volume_ms_entry->ms.uuid, export_config_ms_entry->export_config_ms.uuid);
        strcpy(volume_ms_entry->ms.host, export_config_ms_entry->export_config_ms.host);
        volume_ms_entry->ms.capacity = 0;

        list_push_back(&exportd_volume.mss, &volume_ms_entry->list);
    }

    if ((errno = pthread_rwlock_unlock(&exportd_volume.lock)) != 0) {
        status = -1;
        goto out;
    }

    volume_balance(&exportd_volume);

    // clear and load vfss
    list_for_each_forward_safe(p, q, &exportd_vfss) {
        exportd_vfs_entry_t *entry = list_entry(p, exportd_vfs_entry_t, list);
        list_remove(p);
        free(entry);
    }

    list_for_each_forward(p, &config->mfss) {
        export_config_mfs_entry_t *entry = list_entry(p, export_config_mfs_entry_t, list);
        exportd_vfs_entry_t *exportd_vfs_entry;
        uuid_t uuid;

        if ((exportd_vfs_entry = malloc(sizeof (exportd_vfs_entry_t))) == NULL) {
            status = -1;
            goto out;
        }
        if ((status = vfs_uuid(entry->export_config_mfs, uuid)) != 0) {
            fatal("vfs_uuid failed for export %s: %s", entry->export_config_mfs, strerror(errno));
            fprintf(stderr, "vfs_uuid failed for export %s: %s\n", entry->export_config_mfs, strerror(errno));
            goto out;
        }
        uuid_copy(exportd_vfs_entry->uuid, uuid);
        strcpy(exportd_vfs_entry->vfs.root, entry->export_config_mfs);
        exportd_vfs_entry->vfs.volume = &exportd_volume;

        list_push_back(&exportd_vfss, &exportd_vfs_entry->list);
    }

    if ((errno = pthread_rwlock_unlock(&exportd_lock)) != 0) {
        status = -1;
        goto out;
    }

    status = 0;
out:
    return status;
}

static void * balance_volume(void *v) {

    struct timespec ts = {2, 0}; //XXX hard coded

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    for (;;) {
        volume_balance(&exportd_volume);
        nanosleep(&ts, NULL);
    }
}

static int before_start() {
    int fd;
    int status;
    export_config_t config;

    DEBUG_FUNCTION;

    if ((fd = open(exportd_config_file, O_RDWR)) == -1) {
        fprintf(stderr, "exportd failed: configuration file %s: %s\n", exportd_config_file, strerror(errno));
        status = -1;
        goto out;
    }
    close(fd);

    if (export_config_initialize(&config, exportd_config_file) != 0) {
        fprintf(stderr, "exportd failed: can't initialize configuration file: %s: %s\n", exportd_config_file, strerror(errno));
        status = -1;
        goto out;
    }

    // initialize volume
    if (volume_initialize(&exportd_volume) != 0) {
        fprintf(stderr, "exportd failed: can't initialize volume: %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    // initialize vfss
    list_init(&exportd_vfss);

    if (pthread_rwlock_init(&exportd_lock, NULL) != 0) {
        fprintf(stderr, "exportd failed: can't initialize exportd lock %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    if (load_config(&config) != 0) {
        fprintf(stderr, "exportd failed: wrong data in configuration file: %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    if (export_config_release(&config) != 0) {
        fprintf(stderr, "exportd failed: can't release config file %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    if (pthread_create(&exportd_thread, NULL, balance_volume, NULL) != 0) {
        fprintf(stderr, "exportd failed: can't create thread %s\n", strerror(errno));
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

    if (rozo_initialize() != 0) {
        fatal("can't initialise rozo %s", strerror(errno));
        return;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *) one, sizeof (one));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) one, sizeof (one));
    // XXX Buffers sizes hard coded
    exportd_svc = svctcp_create(sock, 16384, 16384);
    if (exportd_svc == NULL) {
        fatal("can't create service %s", strerror(errno));
        return;
    }

    pmap_unset(EXPORT_PROGRAM, EXPORT_VERSION); // in case !

    if (!svc_register(exportd_svc, EXPORT_PROGRAM, EXPORT_VERSION, export_program_1, IPPROTO_TCP)) {
        fatal("can't register service %s", strerror(errno));
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

    list_for_each_forward_safe(p, q, &exportd_volume.mss) {
        volume_ms_entry_t *entry = list_entry(p, volume_ms_entry_t, list);
        list_remove(p);
        free(entry);
    }

    volume_release(&exportd_volume);

    list_for_each_forward_safe(p, q, &exportd_vfss) {
        exportd_vfs_entry_t *entry = list_entry(p, exportd_vfs_entry_t, list);
        list_remove(p);
        free(entry);
    }
    info("stopped.");
}

static void on_usr1() {

    export_config_t config;

    DEBUG_FUNCTION;

    if (export_config_initialize(&config, exportd_config_file) != 0) {
        fatal("can't initialize config file %s", strerror(errno));
        return;
    }

    if (load_config(&config) != 0) {
        fatal("can't load config %s", strerror(errno));
        return;
    }

    if (export_config_release(&config) != 0) {
        fatal("can't release config file %s", strerror(errno));
        return;
    }
}

uuid_t * exportd_lookup_id(const char *path) {
    list_t *iterator;

    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exportd_vfss) {
        exportd_vfs_entry_t *entry = list_entry(iterator, exportd_vfs_entry_t, list);
        if (strcmp(entry->vfs.root, path) == 0)
            return &entry->uuid;
    }
    errno = EINVAL;
    return NULL;
}

vfs_t * exportd_lookup_vfs(uuid_t uuid) {

    list_t *iterator;

    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &exportd_vfss) {
        exportd_vfs_entry_t *entry = list_entry(iterator, exportd_vfs_entry_t, list);
        if (uuid_compare(entry->uuid, uuid) == 0)
            return &entry->vfs;
    }
    warning("vfs not found.");
    errno = EINVAL;
    return NULL;
}

static void usage() {
    printf("Rozo export daemon - %s\n", VERSION);
    printf("Usage: exportd {--help | -h } | {--create path} | {[{--config | -c} file] --start | --stop | --reload}\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf("\t-c, --config\tconfiguration file to use (default *install prefix*/etc/rozo/export.conf).\n");
    printf("\t--create [path]\tcreate a new export environment.\n");
    printf("\t--start\t\tstart the daemon.\n");
    printf("\t--stop\t\tstop the daemon.\n");
    printf("\t--reload\treload the configuration file (the one used at start time).\n");
    exit(EXIT_FAILURE);
};

int main(int argc, char* argv[]) {

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
                        fprintf(stderr, "exportd failed: export path: %s: %s\n", optarg, strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                break;
            case 'h':
                exportd_command_flag = HELP;
                break;
            case 'c':
                if (!realpath(optarg, exportd_config_file)) {
                    fprintf(stderr, "exportd failed: configuration file: %s: %s\n", optarg, strerror(errno));
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
            if (vfs_create(root) != 0) {
                fprintf(stderr, "exportd failed: export path: %s: %s\n", root, strerror(errno));
            }
            break;
        case START:
            if (before_start() != 0) {
                exit(EXIT_FAILURE);
            }
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
