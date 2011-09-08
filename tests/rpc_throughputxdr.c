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

#include "../tests/rpc_throughput.h"

bool_t xdr_rpc_th_status_t(XDR * xdrs, rpc_th_status_t * objp) {
    register int32_t *buf;

    if (!xdr_enum(xdrs, (enum_t *) objp))
        return FALSE;
    return TRUE;
}

bool_t xdr_rpc_th_status_ret_t(XDR * xdrs, rpc_th_status_ret_t * objp) {
    register int32_t *buf;

    if (!xdr_rpc_th_status_t(xdrs, &objp->status))
        return FALSE;
    switch (objp->status) {
    case RPC_TH_FAILURE:
        if (!xdr_int(xdrs, &objp->rpc_th_status_ret_t_u.error))
            return FALSE;
        break;
    default:
        break;
    }
    return TRUE;
}

bool_t xdr_rpc_th_write_arg_t(XDR * xdrs, rpc_th_write_arg_t * objp) {
    register int32_t *buf;

    if (!xdr_bytes
        (xdrs, (char **) &objp->bins.bins_val,
         (u_int *) & objp->bins.bins_len, ~0))
        return FALSE;
    return TRUE;
}
