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

#include <errno.h>

#include "pool.h"
#include "storage_proto.h"

#define PHT_SIZE 20

typedef struct pool_entry {
    char *host;
    rpc_client_t *client;
    list_t list;
} pool_entry_t;

static unsigned int string_hash(void *key) {
	return hash_table_hash(key, strlen(key));
}

static int string_cmp(void *key1, void *key2) {
	return strcmp((char *)key1, (char *)key2);
}

int pool_initialize(pool_t *pool) {

	int status = -1;

	if (! pool) {
		errno = EFAULT;
		goto out;
	}

    list_init(&pool->connections);
    status = hash_table_init(&pool->ht, PHT_SIZE, string_hash, string_cmp);
out:
	return status;
}

void pool_release(pool_t *pool) {

    list_t *p, *q;

    if (! pool) goto out;

    hash_table_release(&pool->ht);

    list_for_each_forward_safe(p, q, &pool->connections) {
        pool_entry_t *pe = list_entry(p, pool_entry_t, list);
        list_remove(p);
        rpc_client_release(pe->client);
        free(pe->client);
        free(pe->host);
        free(pe);
    }

out:
    return;
}

rpc_client_t * pool_get(pool_t *pool, char* host) {
    
    pool_entry_t *pe = NULL;

    if (pe = hash_table_get(&pool->ht, host)) {
    	goto out;
    }
    
    // if not found.
    if (!(pe = malloc(sizeof(pool_entry_t)))) {
        goto error;
    }
    
    if (!(pe->client = malloc(sizeof(rpc_client_t))))  {
        goto error;
    }
    
    if (!(pe->host = strdup(host))) {
        goto error;
    }

    if (rpc_client_initialize(pe->client, host, STORAGE_PROGRAM, STORAGE_VERSION, 0, 0) != 0) {
    	goto error;
    }

    list_init(&pe->list);

    if (hash_table_put(&pool->ht, pe->host, pe)) {
    	goto error;
    }

    list_push_back(&pool->connections, &pe->list);

    goto out;

error:
	if (pe->client) {
		rpc_client_release(pe->client);
		free(pe->client);
		pe->client = 0;
	}
	if (pe->host) free(pe->host);
	if (pe) free(pe);
	return NULL;
out:
    return pe->client;
}

void pool_discard(pool_t *pool, char* host) {

	pool_entry_t *pe;

	if (pe = hash_table_del(&pool->ht, host)) {
        list_remove(&pe->list);
        rpc_client_release(pe->client);
        free(pe->client);
        free(pe->host);
        free(pe);
	}

	return;
}
