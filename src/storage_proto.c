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

#include <limits.h>
#include <errno.h>

#include "log.h"
#include "rozo.h"
#include "storage.h"
#include "storaged.h"
#include "storage_proto.h"

void * storageproc_null_1_svc(void *noargs, struct svc_req *req) {

    DEBUG("STORAGE PROC NULL OK");
    static char* result;
    return ((void*) &result);
}

storage_status_response_t * storageproc_remove_1_svc(storage_remove_args_t *args, struct svc_req *req) {

    static storage_status_response_t response;
    uint8_t mp;
    storage_t *storage;

    DEBUG_FUNCTION;

    response.status = STORAGE_SUCCESS;
    if ((storage = storaged_lookup(args->uuid)) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_status_response_t_u.error = errno;
        return &response;
    }

    for (mp = 0; mp < ROZO_FORWARD; mp++) {
        if (storage_remove(storage, args->mf, mp) != 0 && errno != ENOENT) {
            response.status = STORAGE_FAILURE;
            response.storage_status_response_t_u.error = errno;
            goto out;
        }
    }

out:
    return &response;
}

storage_status_response_t * storageproc_write_1_svc(storage_write_args_t *args, struct svc_req *req) {

    static storage_status_response_t response;
    storage_t *storage;

    DEBUG_FUNCTION;

    response.status = STORAGE_SUCCESS;
    if ((storage = storaged_lookup(args->uuid)) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_status_response_t_u.error = errno;
        return &response;
    }

    if (storage_write(storage, args->mf, args->mp, args->mb, args->nmbs, args->bins.bins_val) != 0) {
        response.status = STORAGE_FAILURE;
        response.storage_status_response_t_u.error = errno;
        goto out;
    }

out:
    return &response;
}

storage_read_response_t * storageproc_read_1_svc(storage_read_args_t *args, struct svc_req *req) {

    static storage_read_response_t response;
    uint16_t psize;
    storage_t *storage;

    DEBUG_FUNCTION;

    response.status = STORAGE_SUCCESS;

    xdr_free((xdrproc_t) xdr_storage_read_response_t, (char *)&response);

    psize = rozo_psizes[args->mp];

    if ((storage = storaged_lookup(args->uuid)) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_read_response_t_u.error = errno;
        return &response;
    }

    response.storage_read_response_t_u.bins.bins_len = args->nmbs * psize * sizeof(uint8_t);
    if ((response.storage_read_response_t_u.bins.bins_val = malloc(args->nmbs * psize * sizeof(uint8_t))) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_read_response_t_u.error = errno;
        goto out;
    }

    if (storage_read(storage, args->mf, args->mp, args->mb, args->nmbs,
                response.storage_read_response_t_u.bins.bins_val) != 0) {
        response.status = STORAGE_FAILURE;
        response.storage_read_response_t_u.error = errno;
        goto out;
    }

out:
    return &response;
}

storage_status_response_t * storageproc_truncate_1_svc(storage_truncate_args_t *args, struct svc_req *req) {

    static storage_status_response_t response;
    storage_t *storage;

    DEBUG_FUNCTION;

    response.status = STORAGE_SUCCESS;
    if ((storage = storaged_lookup(args->uuid)) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_status_response_t_u.error = errno;
        return &response;
    }

    if (storage_truncate(storage, args->mf, args->mp, args->mb) != 0) {
        response.status = STORAGE_FAILURE;
        response.storage_status_response_t_u.error = errno;
    }

    return &response;
}

storage_stat_response_t * storageproc_stat_1_svc(char *args, struct svc_req *req) {

    static storage_stat_response_t response;
    storage_t *storage;
    sstat_t sstat;

    DEBUG_FUNCTION;

    response.status = STORAGE_SUCCESS;
    if ((storage = storaged_lookup(args)) == NULL) {
        response.status = STORAGE_FAILURE;
        response.storage_stat_response_t_u.error = errno;
        return &response;
    }

    if (storage_stat(storage, &sstat) != 0) {
        response.status = STORAGE_FAILURE;
        response.storage_stat_response_t_u.error = errno;
        goto out;
    }

    response.storage_stat_response_t_u.stat.bsize = sstat.bsize;
    response.storage_stat_response_t_u.stat.bfree = sstat.bfree;

out:
    return &response;
}

