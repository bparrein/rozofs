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

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include "log.h"
#include "list.h"
#include "volume.h"
#include "vfs.h"
#include "exportd.h"
#include "export_proto.h"
#include "storage_proto.h"

void * exportproc_null_1_svc(void *noargs, struct svc_req *req) {

    DEBUG_FUNCTION;
}

export_lookup_response_t * exportproc_lookup_1_svc(export_path_t *root, struct svc_req *req) {

    static export_lookup_response_t response;
    uuid_t *uuid;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((uuid = exportd_lookup_id(*root)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_lookup_response_t_u.error = errno;
    }

    if (uuid != NULL) {
        memcpy(response.export_lookup_response_t_u.uuid, uuid, sizeof (export_uuid_t));
    }

    return &response;
}

export_statfs_response_t * exportproc_statfs_1_svc(export_uuid_t args, struct svc_req *req) {

    static export_statfs_response_t response;
    vfs_t *export;
    struct statvfs stat;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_statfs_response_t_u.error = errno;
        return &response;
    }

    if (vfs_statfs(export, &stat) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_statfs_response_t_u.error = errno;
        return &response;
    }

    response.export_statfs_response_t_u.stat.bsize = stat.f_bsize;
    response.export_statfs_response_t_u.stat.blocks = stat.f_blocks;
    response.export_statfs_response_t_u.stat.bfree = stat.f_bfree;
    response.export_statfs_response_t_u.stat.files = stat.f_files;
    response.export_statfs_response_t_u.stat.ffree = stat.f_ffree;
    response.export_statfs_response_t_u.stat.namemax = stat.f_namemax;

    return &response;
}

export_stat_response_t * exportproc_stat_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_stat_response_t response;
    vfs_t *export;
    struct stat stat;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_stat_response_t_u.error = errno;
        return &response;
    }

    if (vfs_stat(export, args->path, &stat) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_stat_response_t_u.error = errno;
        return &response;
    }

    response.export_stat_response_t_u.stat.mode = stat.st_mode;
    response.export_stat_response_t_u.stat.nlink = stat.st_nlink;
    response.export_stat_response_t_u.stat.ctime = stat.st_ctime;
    response.export_stat_response_t_u.stat.atime = stat.st_atime;
    response.export_stat_response_t_u.stat.mtime = stat.st_mtime;
    response.export_stat_response_t_u.stat.size = stat.st_size;

    return &response;
}

export_readlink_response_t * exportproc_readlink_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_readlink_response_t response;
    vfs_t *export;
    char link[PATH_MAX];

    DEBUG_FUNCTION;

    xdr_free((xdrproc_t) xdr_export_readlink_response_t, (char *) &response);

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_readlink_response_t_u.error = errno;
        return &response;
    }

    if (vfs_readlink(export, args->path, link) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_readlink_response_t_u.error = errno;
        return &response;
    }

    if ((response.export_readlink_response_t_u.path = strdup(link)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_readlink_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_mknod_1_svc(export_mknod_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_mknod(export, args->path, args->mode) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_mkdir_1_svc(export_mkdir_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_mkdir(export, args->path, args->mode) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_unlink_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_unlink(export, args->path) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_rmdir_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_rmdir(export, args->path) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_symlink_1_svc(export_symlink_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_symlink(export, args->target, args->link) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_rename_1_svc(export_rename_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_rename(export, args->from, args->to) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_status_response_t * exportproc_chmod_1_svc(export_chmod_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_chmod(export, args->path, args->mode) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

// do not delete projections on ps (will be done by merkle trees)

export_status_response_t * exportproc_trunc_1_svc(export_trunc_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_truncate(export, args->path, args->offset) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_list_response_t * exportproc_list_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_list_response_t response;
    DIR *dp;
    struct dirent *ep;
    export_children_t *children;
    vfs_t *export;

    DEBUG_FUNCTION;

    xdr_free((xdrproc_t) xdr_export_list_response_t, (char *) &response);

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_list_response_t_u.error = errno;
        return &response;
    }

    children = &response.export_list_response_t_u.children;
    dp = vfs_opendir(export, args->path);
    if (dp != NULL) {
        while (ep = readdir(dp)) {
            if ((*children = malloc(sizeof (export_child_t))) == NULL) {
                response.status = EXPORT_FAILURE;
                response.export_list_response_t_u.error = errno;
                return &response;
            }
            if (((*children)->file = strdup(ep->d_name)) == NULL) {
                response.status = EXPORT_FAILURE;
                response.export_list_response_t_u.error = errno;
                return &response;
            }
            children = &(*children)->next;
        }
        closedir(dp);
        *children = NULL;
    } else {
        response.status = EXPORT_FAILURE;
        response.export_list_response_t_u.error = errno;
        return &response;
    }

    return &response;
}

export_attr_response_t * exportproc_attr_1_svc(export_path_args_t *args, struct svc_req *req) {

    static export_attr_response_t response;
    vattr_t attr;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_attr_response_t_u.error = errno;
        return &response;
    }

    if (vfs_attr(export, args->path, &attr) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_attr_response_t_u.error = errno;
        return &response;
    }

    memcpy(&response.export_attr_response_t_u.attr, &attr, sizeof (vattr_t));

    return &response;
}

export_io_response_t * exportproc_read_1_svc(export_io_args_t *args, struct svc_req *req) {

    static export_io_response_t response;
    int32_t len;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_io_response_t_u.error = errno;
        return &response;
    }

    if ((len = vfs_read(export, args->path, args->offset, args->length)) == -1) {
        response.status = EXPORT_FAILURE;
        response.export_io_response_t_u.error = errno;
        return &response;
    }

    response.export_io_response_t_u.length = len;

    return &response;
}

export_read_block_response_t * exportproc_read_block_1_svc(export_read_block_args_t *args, struct svc_req *req) {

    static export_read_block_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_read_block_response_t_u.error = errno;
        return &response;
    }

    response.export_read_block_response_t_u.distribution.distribution_len = args->nmbs;
    response.export_read_block_response_t_u.distribution.distribution_val = malloc(args->nmbs * 
            sizeof(distribution_t));

    if (vfs_read_block(export, args->path, args->mb, args->nmbs,
            response.export_read_block_response_t_u.distribution.distribution_val) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_read_block_response_t_u.error = errno;
    }

    return &response;
}

export_io_response_t * exportproc_write_1_svc(export_io_args_t *args, struct svc_req *req) {

    static export_io_response_t response;
    int32_t len;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_io_response_t_u.error = errno;
        return &response;
    }

    if ((len = vfs_write(export, args->path, args->offset, args->length)) == -1) {
        response.status = EXPORT_FAILURE;
        response.export_io_response_t_u.error = errno;
        return &response;
    }

    response.export_io_response_t_u.length = len;

    return &response;
}

export_status_response_t * exportproc_write_block_1_svc(export_write_block_args_t *args, struct svc_req *req) {

    static export_status_response_t response;
    vfs_t *export;

    DEBUG_FUNCTION;

    response.status = EXPORT_SUCCESS;

    if ((export = exportd_lookup_vfs(args->uuid)) == NULL) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
        return &response;
    }

    if (vfs_write_block(export, args->path, args->mb, args->nmbs, args->distribution) != 0) {
        response.status = EXPORT_FAILURE;
        response.export_status_response_t_u.error = errno;
    }

    return &response;
}
