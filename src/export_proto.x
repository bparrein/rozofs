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

enum export_status_t {
    EXPORT_SUCCESS = 0,
    EXPORT_FAILURE = 1
};

typedef opaque export_uuid_t[ROZO_UUID_SIZE];

union export_status_response_t switch (export_status_t status) {
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

typedef char export_hostname_t[ROZO_HOSTNAME_MAX];

union export_lookup_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_uuid_t uuid;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

struct export_statfs_t {
    uint16_t bsize;
    uint64_t blocks;
    uint64_t bfree;
    uint64_t files;
    uint64_t ffree;
    uint16_t namemax;
};

union export_statfs_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_statfs_t stat;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

typedef string export_filename_t<ROZO_FILENAME_MAX>;

typedef string export_path_t<ROZO_PATH_MAX>;

struct export_path_args_t {
    export_uuid_t uuid;
    export_path_t path;
};

struct export_stat_t {
    uint32_t mode;
    uint16_t nlink; /* child count for directory */
    uint64_t ctime;
    uint64_t atime;
    uint64_t mtime;
    uint64_t size;
};

union export_stat_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_stat_t stat;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

union export_readlink_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_path_t path;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

struct export_chmod_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint32_t mode;
};

struct export_mknod_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint32_t mode;
};

struct export_mkdir_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint32_t mode;
};

struct export_symlink_args_t {
    export_uuid_t uuid;
    export_path_t target;
    export_path_t link;
};

typedef struct export_child_t *export_children_t;

struct export_child_t {
    export_filename_t file;
    export_children_t next;
};

union export_list_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_children_t children;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

struct export_rename_args_t {
    export_uuid_t uuid;
    export_path_t from;
    export_path_t to;
};

struct export_attr_t {
    export_uuid_t uuid;
    export_uuid_t uuids[ROZO_SAFE];
    export_hostname_t hosts[ROZO_SAFE];
};

union export_attr_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        export_attr_t attr;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

struct export_io_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint64_t offset;
    uint32_t length;
};

struct export_write_block_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint64_t mb;
    uint32_t nmbs;
    uint16_t distribution;
};

struct export_read_block_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint64_t mb;
    uint32_t nmbs;
};

union export_read_block_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        uint16_t distribution<>;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

union export_io_response_t switch (export_status_t status) {
    case EXPORT_SUCCESS:
        uint32_t length;
    case EXPORT_FAILURE:
        int error;
    default:
        void;
};

struct export_trunc_args_t {
    export_uuid_t uuid;
    export_path_t path;
    uint64_t offset;
};

program EXPORT_PROGRAM {
    version EXPORT_VERSION {

        void
        EXPORTPROC_NULL(void)                               = 0;

        export_lookup_response_t
        EXPORTPROC_LOOKUP(export_path_t)                    = 1;

        export_statfs_response_t
        EXPORTPROC_STATFS(export_uuid_t)                    = 2;

        export_stat_response_t
        EXPORTPROC_STAT(export_path_args_t)                 = 3;

        export_readlink_response_t
        EXPORTPROC_READLINK(export_path_args_t)             = 4;

        export_status_response_t
        EXPORTPROC_MKNOD(export_mknod_args_t)               = 5;

        export_status_response_t
        EXPORTPROC_MKDIR(export_mkdir_args_t)               = 6;

        export_status_response_t
        EXPORTPROC_UNLINK(export_path_args_t)               = 7;

        export_status_response_t
        EXPORTPROC_RMDIR(export_path_args_t)                = 8;

        export_status_response_t
        EXPORTPROC_SYMLINK(export_symlink_args_t)           = 9;

        export_status_response_t
        EXPORTPROC_RENAME(export_rename_args_t)             = 10;

        export_status_response_t
        EXPORTPROC_CHMOD(export_chmod_args_t)               = 11;

        export_status_response_t
        EXPORTPROC_TRUNC(export_trunc_args_t)               = 12;

        export_list_response_t
        EXPORTPROC_LIST(export_path_args_t)                 = 13;

        export_attr_response_t
        EXPORTPROC_ATTR(export_path_args_t)                 = 14; 

        export_io_response_t
        EXPORTPROC_READ(export_io_args_t)                   = 15;

        export_read_block_response_t
        EXPORTPROC_READ_BLOCK(export_read_block_args_t)     = 16;

        export_io_response_t
        EXPORTPROC_WRITE(export_io_args_t)                  = 17;

        export_status_response_t
        EXPORTPROC_WRITE_BLOCK(export_write_block_args_t)   = 18;

    } = 1;
} = 0x20000003;

