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

#ifndef _ROZOFS_H
#define _ROZOFS_H

#include <stdint.h>
#include <uuid/uuid.h>
#include "config.h"

#define ROZOFS_UUID_SIZE 16
#define ROZOFS_HOSTNAME_MAX 128
#define ROZOFS_BSIZE 8192       // could it be export specific ?
#define ROZOFS_SAFE_MAX 16
#define ROZOFS_DIR_SIZE 4096
#define ROZOFS_PATH_MAX 4096
#define ROZOFS_FILENAME_MAX 255
#define ROZOFS_CLUSTERS_MAX 16
#define ROZOFS_STORAGES_MAX 64
//#define ROZOFS_MAX_RETRY 5
#define ROZOFS_MD5_SIZE 22
#define ROZOFS_MD5_NONE "0000000000000000000000"

typedef enum {
    LAYOUT_2_3_4, LAYOUT_4_6_8, LAYOUT_8_12_16
} rozofs_layout_t;

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
    sid_t sids[ROZOFS_SAFE_MAX];        // not used for non regular files
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
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
extern uint8_t rozofs_safe;
extern uint8_t rozofs_forward;
extern uint8_t rozofs_inverse;
extern angle_t *rozofs_angles;
extern uint16_t *rozofs_psizes;

int rozofs_initialize(rozofs_layout_t layout);

void rozofs_release();

#endif
