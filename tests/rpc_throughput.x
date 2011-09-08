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

enum rpc_th_status_t {
    RPC_TH_SUCCESS = 0,
    RPC_TH_FAILURE = 1
};

union rpc_th_status_ret_t switch (rpc_th_status_t status) {
    case RPC_TH_FAILURE:    int error;
    default:            void;
};

struct rpc_th_write_arg_t {
    opaque      bins<>;
};

program RPC_THROUGHPUT_PROGRAM {
    version RPC_THROUGHPUT_VERSION {
        void
        RPC_TH_NULL(void)                   = 0;

        rpc_th_status_ret_t
        RPC_TH_WRITE(rpc_th_write_arg_t)        = 1;

    }=1;
} = 0x20000005;

