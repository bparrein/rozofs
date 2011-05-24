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


#ifndef _STORAGECLT_H
#define _STORAGECLT_H

#include <uuid/uuid.h>
#include "rozo.h"
#include "rpcclt.h"
#include "storage.h"

typedef struct storageclt {
    char host[ROZO_HOSTNAME_MAX];
    sid_t sid;
    rpcclt_t rpcclt;
} storageclt_t;

int storageclt_initialize(storageclt_t * clt, const char *host, sid_t sid);

void storageclt_release(storageclt_t * clt);

int storageclt_stat(storageclt_t * clt, sstat_t * st);

int storageclt_write(storageclt_t * clt, fid_t fid, tid_t tid, bid_t bid,
                     uint32_t nrb, const bin_t * bins);

int storageclt_read(storageclt_t * clt, fid_t fid, tid_t tid, bid_t bid,
                    uint32_t nrb, bin_t * bins);

int storageclt_truncate(storageclt_t * clt, fid_t fid, tid_t tid, bid_t bid);

int storageclt_remove(storageclt_t * clt, fid_t fid, tid_t tid);

#endif
