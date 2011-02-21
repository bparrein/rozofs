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

#ifndef _HASH_TABLE
#define _HASH_TABLE

#include "list.h"

typedef struct hash_table {
	unsigned int (*hash) (void *);
	int (*cmp) (void *, void *);
	int size;
	list_t *buckets;
} hash_table_t;

// Convenient function hashing memory chunk.
// Could be used by user defined hashing function.
unsigned int hash_table_hash(void *key, int len);

int hash_table_init(hash_table_t *h, int size, unsigned int (*hash) (void *), int (*cmp) (void *, void *));

void hash_table_release(hash_table_t *h);

int hash_table_put(hash_table_t *h, void *key, void *value);

void * hash_table_get(hash_table_t *h, void *key);

void * hash_table_del(hash_table_t *h, void *key);

#endif
