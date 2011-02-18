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

#ifndef _POOL_H
#define _POOL_H

#include "list.h"
#include "hash_table.h"
#include "rpc_client.h"

typedef struct pool {
	list_t connections;
	hash_table_t ht;
} pool_t;

int pool_initialize(pool_t *pool);

void pool_release(pool_t *pool);

rpc_client_t * pool_get(pool_t *pool, char* host);

void pool_discard(pool_t *pool, char* host);


#endif
