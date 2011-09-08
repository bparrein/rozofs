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

#ifndef _HTABLE
#define _HTABLE

#include <stdint.h>
#include "rozofs.h"
#include "list.h"

typedef struct htable {
    uint32_t(*hash) (void *);
    int (*cmp) (void *, void *);
    uint32_t size;
    list_t *buckets;
} htable_t;

void htable_initialize(htable_t * h, uint32_t size, uint32_t(*hash) (void *),
                       int (*cmp) (void *, void *));

void htable_release(htable_t * h);

void htable_put(htable_t * h, void *key, void *value);

void *htable_get(htable_t * h, void *key);

void *htable_del(htable_t * h, void *key);

#endif
