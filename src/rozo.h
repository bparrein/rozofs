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

#ifndef _ROZO_H
#define _ROZO_H

#include <stdint.h>
#include <uuid/uuid.h>
#include "config.h"

#define ROZO_UUID_SIZE 16
#define ROZO_HOSTNAME_MAX 128
#define ROZO_BSIZE 8192         // could it be export specific ?
#define ROZO_SAFE_MAX 16
#define ROZO_DIR_SIZE 4096
#define ROZO_PATH_MAX 4096
#define ROZO_FILENAME_MAX 4096
#define ROZO_CLUSTERS_MAX 50
#define ROZO_STORAGES_MAX 50
//#define ROZO_MAX_RETRY 5

typedef enum {
    LAYOUT_2_3_4, LAYOUT_4_6_8, LAYOUT_8_12_16
} rozo_layout_t;

typedef uint8_t tid_t;          // projection id
typedef uint64_t bid_t;         // block id
typedef uuid_t fid_t;           // file id
typedef uint16_t sid_t;         // storage id
typedef uint16_t cid_t;         // cluster id
typedef uint32_t eid_t;         // export id

// storage stat

typedef struct sstat {
    uint64_t size;
    uint64_t free;
} sstat_t;

// meta file attr

typedef struct mattr {
    fid_t fid;
    cid_t cid;                  // 0 for non regular files
    sid_t sids[ROZO_SAFE_MAX];  // not used for non regular files
    uint32_t mode;
    uint16_t nlink;
    uint64_t ctime;
    uint64_t atime;
    uint64_t mtime;
    uint64_t size;
} mattr_t;

typedef struct estat {
    uint16_t bsize;
    uint64_t blocks;
    uint64_t bfree;
    uint64_t files;
    uint64_t ffree;
    uint16_t namemax;
} estat_t;

typedef struct child {
    char *name;
    struct child *next;
} child_t;

#include "transform.h"
extern uint8_t rozo_safe;
extern uint8_t rozo_forward;
extern uint8_t rozo_inverse;
extern angle_t *rozo_angles;
extern uint16_t *rozo_psizes;

int rozo_initialize(rozo_layout_t layout);

void rozo_release();

#endif
