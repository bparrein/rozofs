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

#ifndef _RPC_THROUGHPUT_H_RPCGEN
#define _RPC_THROUGHPUT_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif


enum rpc_th_status_t {
	RPC_TH_SUCCESS = 0,
	RPC_TH_FAILURE = 1,
};
typedef enum rpc_th_status_t rpc_th_status_t;

struct rpc_th_status_ret_t {
	rpc_th_status_t status;
	union {
		int error;
	} rpc_th_status_ret_t_u;
};
typedef struct rpc_th_status_ret_t rpc_th_status_ret_t;

struct rpc_th_write_arg_t {
	struct {
		u_int bins_len;
		char *bins_val;
	} bins;
};
typedef struct rpc_th_write_arg_t rpc_th_write_arg_t;

#define RPC_THROUGHPUT_PROGRAM 0x20000005
#define RPC_THROUGHPUT_VERSION 1

#if defined(__STDC__) || defined(__cplusplus)
#define RPC_TH_NULL 0
extern  void * rpc_th_null_1(void *, CLIENT *);
extern  void * rpc_th_null_1_svc(void *, struct svc_req *);
#define RPC_TH_WRITE 1
extern  rpc_th_status_ret_t * rpc_th_write_1(rpc_th_write_arg_t *, CLIENT *);
extern  rpc_th_status_ret_t * rpc_th_write_1_svc(rpc_th_write_arg_t *, struct svc_req *);
extern int rpc_throughput_program_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define RPC_TH_NULL 0
extern  void * rpc_th_null_1();
extern  void * rpc_th_null_1_svc();
#define RPC_TH_WRITE 1
extern  rpc_th_status_ret_t * rpc_th_write_1();
extern  rpc_th_status_ret_t * rpc_th_write_1_svc();
extern int rpc_throughput_program_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_rpc_th_status_t (XDR *, rpc_th_status_t*);
extern  bool_t xdr_rpc_th_status_ret_t (XDR *, rpc_th_status_ret_t*);
extern  bool_t xdr_rpc_th_write_arg_t (XDR *, rpc_th_write_arg_t*);

#else /* K&R C */
extern bool_t xdr_rpc_th_status_t ();
extern bool_t xdr_rpc_th_status_ret_t ();
extern bool_t xdr_rpc_th_write_arg_t ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_RPC_THROUGHPUT_H_RPCGEN */
