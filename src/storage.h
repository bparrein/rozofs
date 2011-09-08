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

#ifndef _STORAGE_H
#define _STORAGE_H

#include <stdint.h>
#include <limits.h>
#include <uuid/uuid.h>
#include "rozofs.h"
#include "list.h"
#include "htable.h"

typedef struct storage {
    sid_t sid;
    char root[PATH_MAX];
    uint32_t csize;
    list_t pfiles;
    htable_t htable;
} storage_t;

int storage_initialize(storage_t * st, sid_t sid, const char *root);

void storage_release(storage_t * st);

int storage_write(storage_t * st, fid_t fid, tid_t pid, bid_t bid, uint32_t n,
                  const bin_t * bins);

int storage_read(storage_t * st, fid_t fid, tid_t pid, bid_t bid, uint32_t n,
                 bin_t * bins);

int storage_truncate(storage_t * st, fid_t fid, tid_t pid, bid_t bid);

int storage_rm_file(storage_t * st, fid_t fid);

int storage_stat(storage_t * st, sstat_t * sstat);

#endif
