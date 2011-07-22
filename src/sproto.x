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

typedef unsigned char sp_uuid_t[ROZO_UUID_SIZE];

enum sp_status_t {
    SP_SUCCESS = 0,
    SP_FAILURE = 1
};

union sp_status_ret_t switch (sp_status_t status) {
    case SP_FAILURE:    int error;
    default:            void;
};

struct sp_remove_arg_t {
    uint16_t    sid;
    sp_uuid_t   fid;
};

struct sp_write_arg_t {
    uint16_t    sid;
    sp_uuid_t   fid; 
    uint8_t     tid; 
    uint64_t    bid; 
    uint32_t    nrb; 
    opaque      bins<>;
};

struct sp_read_arg_t {
    uint16_t    sid;
    sp_uuid_t   fid; 
    uint8_t     tid; 
    uint64_t    bid;
    uint32_t    nrb;
};

struct sp_truncate_arg_t {
    uint16_t    sid;
    sp_uuid_t   fid; 
    uint8_t     tid; 
    uint64_t    bid; 
};

union sp_read_ret_t switch (sp_status_t status) {
    case SP_SUCCESS:    opaque  bins<>;
    case SP_FAILURE:    int     error;
    default:            void;
};

struct sp_sstat_t {
    uint64_t size;
    uint64_t free;
};

union sp_stat_ret_t switch (sp_status_t status) {
    case SP_SUCCESS:    sp_sstat_t  sstat;
    case SP_FAILURE:    int         error;
    default:            void;
};

program STORAGE_PROGRAM {
    version STORAGE_VERSION {
        void
        SP_NULL(void)                   = 0;

        sp_status_ret_t
        SP_REMOVE(sp_remove_arg_t)      = 1;

        sp_status_ret_t
        SP_WRITE(sp_write_arg_t)        = 2;

        sp_read_ret_t
        SP_READ(sp_read_arg_t)          = 3;

        sp_status_ret_t
        SP_TRUNCATE(sp_truncate_arg_t)  = 4;

        sp_stat_ret_t
        SP_STAT(uint16_t)               = 5;

    }=1;
} = 0x20000002;

