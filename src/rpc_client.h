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

#ifndef _RPC_CLIENT_H
#define _RPC_CLIENT_H 1

#include <rpc/rpc.h>

typedef struct rpc_client {
    int sock;
    CLIENT *client;
} rpc_client_t;


int rpc_client_initialize(rpc_client_t *client, const char *host, unsigned long prog, unsigned long vers, 
        unsigned int sendsz, unsigned int recvsz);

int rpc_client_release(rpc_client_t *client);

#endif
