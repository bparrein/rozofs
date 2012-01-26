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
#include <sys/resource.h>
#include <unistd.h>
#include <libintl.h>
#include <sys/poll.h>
#include <rpc/rpc.h>
#include <getopt.h>
#include <rpc/pmap_clnt.h>
#include <libconfig.h>

#include "config.h"
#include "xmalloc.h"
#include "log.h"
#include "list.h"
#include "daemon.h"
#include "storage.h"
#include "sproto.h"

#define STORAGED_PID_FILE "storaged.pid"

static char storaged_config_file[PATH_MAX] = STORAGED_DEFAULT_CONFIG;
static int storaged_command_flag = -1;
static storage_t *storaged_storages = 0;
static uint16_t storaged_nrstorages = 0;
extern void storage_program_1(struct svc_req *rqstp, SVCXPRT * ctl_svc);
static SVCXPRT *storaged_svc = 0;

storage_t *storaged_lookup(sid_t sid) {
    storage_t *st = 0;
    DEBUG_FUNCTION;

    st = storaged_storages;
    do {
        if (st->sid == sid)
            goto out;
    } while (st++ != storaged_storages + storaged_nrstorages);
    errno = EINVAL;
    st = 0;
out:
    return st;
}

static void storaged_release() {
    DEBUG_FUNCTION;

    if (storaged_storages) {
        storage_t *st = storaged_storages;
        while (st != storaged_storages + storaged_nrstorages)
            storage_release(st++);
        free(storaged_storages);
        storaged_nrstorages = 0;
        storaged_storages = 0;
    }
}

static int load_layout_conf(struct config_t *config) {
    int status = -1;
    long int layout;

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

static int load_storages_conf(struct config_t *config) {

    int status = -1;
    int i = 0;
    struct config_setting_t *settings = NULL;

    if (!(settings = config_lookup(config, "storages"))) {
        errno = ENOKEY;
        fprintf(stderr, "can't locate the storages settings in conf file\n");
        fatal("can't locate the storages settings in conf file");
        goto out;
    }

    storaged_storages =
        xmalloc(config_setting_length(settings) * sizeof (storage_t));

    for (i = 0; i < config_setting_length(settings); i++) {
        struct config_setting_t *ms = NULL;
        long int sid;
        const char *root;

        if (!(ms = config_setting_get_elem(settings, i))) {
            errno = EIO;        //XXX
            fprintf(stderr, "cant't fetche storage at index %d\n", i);
            severe("cant't fetche storage at index %d", i);
            goto out;
        }

        if (config_setting_lookup_int(ms, "sid", &sid) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up sid for storage (idx=%d)\n", i);
            fatal("cant't look up sid for storage (idx=%d)", i);
            goto out;
        }

        if (storaged_lookup(sid) != NULL) {
            fprintf(stderr,
                    "cant't add storage with sid %u: already exists\n", sid);
            info("cant't add storage with sid %u: already exists", sid);
            goto out;
        }

        if (config_setting_lookup_string(ms, "root", &root) == CONFIG_FALSE) {
            errno = ENOKEY;
            fprintf(stderr, "cant't look up root path for storage (idx=%d)\n",
                    i);
            severe("cant't look up root path for storage (idx=%d)", i);
            goto out;
        }

        if (storage_initialize(storaged_storages + i, (uint16_t) sid, root) !=
            0) {
            fprintf(stderr,
                    "can't initialize storage (sid:%u) with path %s: %s\n",
                    sid, root, strerror(errno));
            severe("can't initialize storage (sid:%u) with path %s: %s", sid,
                   root, strerror(errno));
            goto out;
        }

        storaged_nrstorages++;
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

    if ((fd = open(storaged_config_file, O_RDWR)) == -1) {
        fprintf(stderr, "can't load config file %s: %s\n",
                storaged_config_file, strerror(errno));
        fatal("can't load config file %s: %s", storaged_config_file,
              strerror(errno));
        status = -1;
        goto out;
    }
    close(fd);

    if (config_read_file(&config, storaged_config_file) == CONFIG_FALSE) {
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

    if (load_storages_conf(&config) != 0) {
        goto out;
    }

    status = 0;

out:
    config_destroy(&config);
    return status;
}

static int storaged_initialize() {
    int status = -1;
    DEBUG_FUNCTION;

    // Load configuration
    if (load_conf_file() != 0) {
        fprintf(stderr, "can't load settings from config file\n");
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

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    setsockopt(sock, SOL_TCP, TCP_NODELAY, (char *) &one, sizeof (int));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (int));

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

    if ((storaged_svc =
         svctcp_create(sock, ROZOFS_RPC_BUFFER_SIZE,
                       ROZOFS_RPC_BUFFER_SIZE)) == NULL) {
        fatal("can't create service.");
        return;
    }

    pmap_unset(STORAGE_PROGRAM, STORAGE_VERSION);       // in case !

    if (!svc_register
        (storaged_svc, STORAGE_PROGRAM, STORAGE_VERSION, storage_program_1,
         IPPROTO_TCP)) {
        fatal("can't register service : %s", strerror(errno));
        return;
    }

    info("running.");
    svc_run();
}

static void on_stop() {
    DEBUG_FUNCTION;

    svc_exit();
    svc_unregister(STORAGE_PROGRAM, STORAGE_VERSION);
    pmap_unset(STORAGE_PROGRAM, STORAGE_VERSION);
    if (storaged_svc) {
        svc_destroy(storaged_svc);
        storaged_svc = NULL;
    }
    storaged_release();
    rozofs_release();

    info("stopped.");
}

void usage() {
    printf("Rozofs storage daemon - %s\n", VERSION);
    printf("Usage: storaged [OPTIONS]\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf
        ("\t-c, --config\tconfig file to use (default *install prefix*/etc/rozofs/storage.conf).\n");
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
        case 0:                //long option (manage by getopt but we don't want to be catched by default label)
            break;
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
            break;
        case 'c':
            if (!realpath(optarg, storaged_config_file)) {
                fprintf(stderr,
                        "storaged failed: configuration file: %s: %s\n",
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

    openlog("storaged", LOG_PID, LOG_DAEMON);

    if (storaged_initialize() != 0) {
        fprintf(stderr, "storaged start failed\n");
        exit(EXIT_FAILURE);
    }
    daemon_start(STORAGED_PID_FILE, on_start, on_stop, NULL);

    exit(0);
}
