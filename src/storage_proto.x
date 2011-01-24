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

%#include "rozo.h"

enum storage_status_t {
    STORAGE_SUCCESS = 0,
    STORAGE_FAILURE = 1
};

typedef opaque storage_uuid_t[ROZO_UUID_SIZE];

union storage_status_response_t switch (storage_status_t status) {
    case STORAGE_FAILURE:
        int error;
    default:
        void;
};

struct storage_remove_args_t {
    storage_uuid_t   uuid;
    storage_uuid_t   mf; 
};

struct storage_write_args_t {
    storage_uuid_t   uuid;
    storage_uuid_t   mf; 
    uint8_t     mp; 
    uint64_t    mb; 
    uint32_t    nmbs; 
    opaque     bins<>;
};

struct storage_read_args_t {
    storage_uuid_t   uuid;
    storage_uuid_t   mf; 
    uint8_t     mp; 
    uint64_t    mb;
    uint32_t    nmbs;
};

struct storage_truncate_args_t {
    storage_uuid_t   uuid;
    storage_uuid_t   mf; 
    uint8_t     mp; 
    uint64_t    mb; 
};

union storage_read_response_t switch (storage_status_t status) {
    case STORAGE_SUCCESS:
        opaque bins<>;
    case STORAGE_FAILURE:
        int error;
    default:
        void;
};

struct storage_stat_t {
    uint32_t    bsize;
    uint64_t    bfree;
};

union storage_stat_response_t switch (storage_status_t status) {
    case STORAGE_SUCCESS:
        storage_stat_t stat;
    case STORAGE_FAILURE:
        int error;
    default:
        void;
};

program STORAGE_PROGRAM {
    version STORAGE_VERSION {
        void
        STORAGEPROC_NULL(void)                          = 0;

        storage_status_response_t
        STORAGEPROC_REMOVE(storage_remove_args_t)       = 1;

        storage_status_response_t
        STORAGEPROC_WRITE(storage_write_args_t)         = 2;

        storage_read_response_t
        STORAGEPROC_READ(storage_read_args_t)           = 3;

        storage_status_response_t
        STORAGEPROC_TRUNCATE(storage_truncate_args_t)   = 4;

        storage_stat_response_t
        STORAGEPROC_STAT(storage_uuid_t)                = 5;

    }=1;
} = 0x20000001;

