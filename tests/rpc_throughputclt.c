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

#include <memory.h>             /* for memset */
#include "../tests/rpc_throughput.h"

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

void *rpc_th_null_1(void *argp, CLIENT * clnt) {
    static char clnt_res;

    memset((char *) &clnt_res, 0, sizeof (clnt_res));
    if (clnt_call
        (clnt, RPC_TH_NULL, (xdrproc_t) xdr_void, (caddr_t) argp,
         (xdrproc_t) xdr_void, (caddr_t) & clnt_res,
         TIMEOUT) != RPC_SUCCESS) {
        return (NULL);
    }
    return ((void *) &clnt_res);
}

rpc_th_status_ret_t *rpc_th_write_1(rpc_th_write_arg_t * argp, CLIENT * clnt) {
    static rpc_th_status_ret_t clnt_res;

    memset((char *) &clnt_res, 0, sizeof (clnt_res));
    if (clnt_call
        (clnt, RPC_TH_WRITE, (xdrproc_t) xdr_rpc_th_write_arg_t,
         (caddr_t) argp, (xdrproc_t) xdr_rpc_th_status_ret_t,
         (caddr_t) & clnt_res, TIMEOUT) != RPC_SUCCESS) {
        return (NULL);
    }
    return (&clnt_res);
}
