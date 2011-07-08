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
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../tests/rpc_throughput.h"

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

void rpc_throughput_program_1(struct svc_req *rqstp,
                              register SVCXPRT * transp) {

    union {
        rpc_th_write_arg_t rpc_th_write_1_arg;
    } argument;
    char *result;
    xdrproc_t _xdr_argument, _xdr_result;
    char *(*local) (char *, struct svc_req *);

    switch (rqstp->rq_proc) {
    case RPC_TH_NULL:
        _xdr_argument = (xdrproc_t) xdr_void;
        _xdr_result = (xdrproc_t) xdr_void;
        local = (char *(*)(char *, struct svc_req *)) rpc_th_null_1_svc;
        break;

    case RPC_TH_WRITE:
        _xdr_argument = (xdrproc_t) xdr_rpc_th_write_arg_t;
        _xdr_result = (xdrproc_t) xdr_rpc_th_status_ret_t;
        local = (char *(*)(char *, struct svc_req *)) rpc_th_write_1_svc;
        break;

    default:
        svcerr_noproc(transp);
        return;
    }
    memset((char *) &argument, 0, sizeof (argument));
    if (!svc_getargs(transp, (xdrproc_t) _xdr_argument, (caddr_t) & argument)) {
        svcerr_decode(transp);
        return;
    }
    result = (*local) ((char *) &argument, rqstp);
    if (result != NULL &&
        !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
        svcerr_systemerr(transp);
    }
    if (!svc_freeargs
        (transp, (xdrproc_t) _xdr_argument, (caddr_t) & argument)) {
        fprintf(stderr, "%s", "unable to free arguments");
        exit(1);
    }
    return;
}

int main(int argc, char **argv) {
    int sock;
    int one = 1;
    register SVCXPRT *transp;

    pmap_unset(RPC_THROUGHPUT_PROGRAM, RPC_THROUGHPUT_VERSION);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_TCP, TCP_NODELAY, (char *) &one, sizeof (int));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (int));

    if ((transp = svctcp_create(sock, 0, 0)) == NULL) {
        fprintf(stderr, "%s", "cannot create tcp service.");
        exit(1);
    }

    if (!svc_register
        (transp, RPC_THROUGHPUT_PROGRAM, RPC_THROUGHPUT_VERSION,
         rpc_throughput_program_1, IPPROTO_TCP)) {
        fprintf(stderr, "%s",
                "unable to register (RPC_THROUGHPUT_PROGRAM, RPC_THROUGHPUT_VERSION, tcp).");
        exit(1);
    }

    svc_run();
    fprintf(stderr, "%s", "svc_run returned");
    exit(1);
    /* NOTREACHED */
}
