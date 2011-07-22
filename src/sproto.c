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
#include "storage.h"
#include "storaged.h"
#include "sproto.h"
#include "xmalloc.h"

void *sp_null_1_svc(void *args, struct svc_req *req) {
    DEBUG_FUNCTION;
    return 0;
}

sp_status_ret_t *sp_remove_1_svc(sp_remove_arg_t * args, struct svc_req * req) {
    static sp_status_ret_t ret;
    storage_t *st = 0;
    DEBUG_FUNCTION;

    ret.status = SP_FAILURE;
    if ((st = storaged_lookup(args->sid)) == 0) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }
    if (storage_rm_file(st, args->fid) != 0 && errno != ENOENT) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }

    ret.status = SP_SUCCESS;
out:
    return &ret;
}

sp_status_ret_t *sp_write_1_svc(sp_write_arg_t * args, struct svc_req * req) {
    static sp_status_ret_t ret;
    storage_t *st = 0;
    DEBUG_FUNCTION;

    ret.status = SP_FAILURE;
    if ((st = storaged_lookup(args->sid)) == 0) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }
    if (storage_write
        (st, args->fid, args->tid, args->bid, args->nrb,
         (bin_t *) args->bins.bins_val) != 0) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }
    ret.status = SP_SUCCESS;
out:
    return &ret;
}

sp_read_ret_t *sp_read_1_svc(sp_read_arg_t * args, struct svc_req * req) {
    static sp_read_ret_t ret;
    uint32_t psize;
    storage_t *st = 0;
    DEBUG_FUNCTION;

    xdr_free((xdrproc_t) xdr_sp_read_ret_t, (char *) &ret);
    ret.status = SP_FAILURE;

    if ((st = storaged_lookup(args->sid)) == 0) {
        ret.sp_read_ret_t_u.error = errno;
        goto out;
    }
    psize = rozo_psizes[args->tid];
    ret.sp_read_ret_t_u.bins.bins_len = args->nrb * psize * sizeof (bin_t);
    ret.sp_read_ret_t_u.bins.bins_val =
        (char *) xmalloc(args->nrb * psize * sizeof (bin_t));
    if (storage_read
        (st, args->fid, args->tid, args->bid, args->nrb,
         (bin_t *) ret.sp_read_ret_t_u.bins.bins_val) != 0) {
        ret.sp_read_ret_t_u.error = errno;
        goto out;
    }
    ret.status = SP_SUCCESS;
out:
    return &ret;
}

sp_status_ret_t *sp_truncate_1_svc(sp_truncate_arg_t * args,
                                   struct svc_req * req) {
    static sp_status_ret_t ret;
    storage_t *st = 0;
    DEBUG_FUNCTION;

    ret.status = SP_FAILURE;
    if ((st = storaged_lookup(args->sid)) == 0) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }
    if (storage_truncate(st, args->fid, args->tid, args->bid) != 0) {
        ret.sp_status_ret_t_u.error = errno;
        goto out;
    }
    ret.status = SP_SUCCESS;
out:
    return &ret;
}

sp_stat_ret_t *sp_stat_1_svc(uint16_t * sid, struct svc_req * req) {
    static sp_stat_ret_t ret;
    storage_t *st = 0;
    sstat_t sstat;
    DEBUG_FUNCTION;

    ret.status = SP_FAILURE;
    if ((st = storaged_lookup(*sid)) == 0) {
        ret.sp_stat_ret_t_u.error = errno;
        goto out;
    }
    if (storage_stat(st, &sstat) != 0) {
        ret.sp_stat_ret_t_u.error = errno;
        goto out;
    }
    ret.sp_stat_ret_t_u.sstat.size = sstat.size;
    ret.sp_stat_ret_t_u.sstat.free = sstat.free;
    ret.status = SP_SUCCESS;
out:
    return &ret;
}
