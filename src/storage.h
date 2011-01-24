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
 
#ifndef _STORAGE_H
#define _STORAGE_H

#include <stdint.h>
#include <limits.h>
#include <uuid/uuid.h>

typedef char storage_t[PATH_MAX];

typedef struct sstat {
    uint16_t bsize;
    uint64_t bfree;
} sstat_t;

int storage_write(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb, uint32_t nmbs, const uint8_t *bins);

int storage_read(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb, uint32_t nmbs, uint8_t *bins);

int storage_truncate(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb);

int storage_remove(storage_t *storage, uuid_t mf, uint8_t mp);

int storage_stat(storage_t *storage, sstat_t *sstat);

#endif
