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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include "rpc_throughput.h"

void *rpc_th_null_1_svc(void *argp, struct svc_req *rqstp) {
    static char *result;

    /*
     * insert server code here
     */

    return (void *) &result;
}

rpc_th_status_ret_t *rpc_th_write_1_svc(rpc_th_write_arg_t * argp,
                                        struct svc_req *rqstp) {
    static char *result;
    fprintf(stderr, "Receiving a request of %u Bytes\n", argp->bins.bins_len);

    return (void *) &result;
}
