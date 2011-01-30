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
#include <sys/resource.h>
#include <unistd.h>
#include <libintl.h>
#include <sys/poll.h>
#include <rpc/rpc.h>
#include <getopt.h>

#include "config.h"
#include "log.h"
#include "list.h"
#include "daemon.h"
#include "storage.h"
#include "storage_proto.h"
#include "storage_config.h"

#define STORAGED_PID_FILE "storaged.pid"

enum command {
    HELP,
    START,
    STOP
};

typedef struct storaged_storage_entry {
    uuid_t uuid;
    storage_t storage;
    list_t list;
} storaged_storage_entry_t;

static list_t storaged_storages;

static char storaged_config_file[PATH_MAX] = STORAGED_DEFAULT_CONFIG;
//static char storaged_storage[PATH_MAX];

static int storaged_command_flag = -1;

extern void storage_program_1(struct svc_req *rqstp, SVCXPRT *ctl_svc);

static SVCXPRT *storaged_svc = NULL;

storage_t * storaged_lookup(uuid_t uuid) {

    list_t *iterator;

    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &storaged_storages) {
        storaged_storage_entry_t *entry = list_entry(iterator, storaged_storage_entry_t, list);
        if (uuid_compare(entry->uuid, uuid) == 0)
            return &entry->storage;
    }
    warning("storage not found.");
    errno = EINVAL;
    return NULL;
}

static void on_start() {

    int sock;
    int one = 1;
    storage_config_t config;
    list_t *p;

    DEBUG_FUNCTION;

    if (rozo_initialize() != 0) {
        fatal("can't initialize rozo: %s.", strerror(errno));
	    return;
    }

    if (storage_config_initialize(&config, storaged_config_file) != 0) {
        fatal("can't initialize config file %s", strerror(errno));
        return;
    }
    
    // initialize storages
    list_init(&storaged_storages);

    list_for_each_forward(p, &config) {
        storage_config_ms_entry_t *storage_config_ms_entry;
        storaged_storage_entry_t *storaged_storage_entry ;
        
        if ((storaged_storage_entry = malloc(sizeof(storaged_storage_entry_t))) == NULL) {
            fatal("can't allocate memory: %s.", strerror(errno));
            return;
        }

        storage_config_ms_entry = list_entry(p, storage_config_ms_entry_t, list);

        uuid_copy(storaged_storage_entry->uuid, storage_config_ms_entry->storage_config_ms.uuid);
        strcpy(storaged_storage_entry->storage, storage_config_ms_entry->storage_config_ms.root);

        list_push_back(&storaged_storages, &storaged_storage_entry->list);
    }

    if (storage_config_release(&config) != 0) {
        fatal("can't release config file: %s.", strerror(errno));
        return;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *) one, sizeof (one));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) one, sizeof (one));
    // XXX Buffers sizes hard coded
    if ((storaged_svc = svctcp_create(sock, 16384, 16384)) == NULL) {
        fatal("can't create service: %s.", strerror(errno));
        return;
    }

    pmap_unset(STORAGE_PROGRAM, STORAGE_VERSION);

    if (! svc_register(storaged_svc, STORAGE_PROGRAM, STORAGE_VERSION, storage_program_1, IPPROTO_TCP)) {
        fatal("can't register service: %s.", strerror(errno));
        return;
    }

    info("running.");
    svc_run();
}

static void on_stop() {

    list_t *p, *q;

    DEBUG_FUNCTION;

    svc_exit();

    svc_unregister(STORAGE_PROGRAM, STORAGE_VERSION);
    pmap_unset(STORAGE_PROGRAM, STORAGE_VERSION);
    if (storaged_svc) {
    	svc_destroy(storaged_svc);
        storaged_svc = NULL;
    }

    list_for_each_forward_safe(p, q, &storaged_storages) {
        storaged_storage_entry_t *entry = list_entry(p, storaged_storage_entry_t, list);
        list_remove(p);
        free(entry);
    }
    info("stopped.");
}

void usage() {
    printf("Rozo storage daemon - %s\n", VERSION);
    printf("Usage: storaged {--help} | {[{--config | -c} file] --start | --stop}\n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf("\t-c, --config\tconfig file to use (default *install prefix*/etc/rozo/storage.conf).\n");
    printf("\t--start\t\tstart the daemon.\n");
    printf("\t--stop\t\tstop the daemon\n");
    exit(EXIT_FAILURE);
};

int main(int argc, char* argv[]) {

    int c;

    static struct option long_options[] = {
        {"help",	no_argument, 	&storaged_command_flag,	HELP},
        {"start",	no_argument,	&storaged_command_flag,	START},
        {"stop",	no_argument,	&storaged_command_flag,	STOP},
        {"config",	required_argument, 	0, 		'c'},
        {0, 0, 0, 0}
    };

    while (1) {
    	int option_index = 0;

    	c = getopt_long (argc, argv, "hc:", long_options, &option_index);
     
        if (c == -1) 
            break;
     
	    switch (c) {
	        case 0: //long option (manage by getopt but we don't want to be catched by default label)
                break;
	        case 'h':
                storaged_command_flag = HELP;
                break;
	        case 'c':
	    	    if (! realpath(optarg, storaged_config_file)) {
	    	        perror("realpath failed");
	    	        exit(EXIT_FAILURE);
                }
	    	    break;
            case '?':
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    }

    switch (storaged_command_flag) {
	    case HELP:
            usage();
            break;
        case START:
            daemon_start(STORAGED_PID_FILE, on_start, on_stop, NULL);
            break;
        case STOP:
            daemon_stop(STORAGED_PID_FILE);
            break;
        default:
            usage();
    }

    exit(0);
}
