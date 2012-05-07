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

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include "log.h"
#include "xmalloc.h"
#include "export.h"
#include "volume.h"
#include "exportd.h"
#include "eproto.h"
#include "sproto.h"
#include <pthread.h>

extern volume_t volume;

void *ep_null_1_svc(void *noargs, struct svc_req *req) {
    DEBUG_FUNCTION;
    return 0;
}

ep_mount_ret_t *ep_mount_1_svc(ep_path_t * arg, struct svc_req * req) {
    static ep_mount_ret_t ret;
    list_t *it_1, *q;
    eid_t *eid = NULL;
    export_t *exp;
    int i = 0;
    int j = 0;
    DEBUG_FUNCTION;

    // XXX exportd_lookup_id could return export_t *
    if (!(eid = exports_lookup_id(*arg)))
        goto error;
    if (!(exp = exports_lookup_export(*eid)))
        goto error;

    if ((errno = pthread_rwlock_rdlock(&volumes_list.lock)) != 0) {
        ret.status = EP_FAILURE;
        goto error;
    }

    list_for_each_forward(it_1, &volumes_list.vol_list) {

        volume_t *entry_vol = list_entry(it_1, volume_t, list);

        // Get volume with this vid
        if (entry_vol->vid == exp->vid) {

            list_for_each_forward(q, &entry_vol->cluster_list) {

                cluster_t *cluster = list_entry(q, cluster_t, list);

                ret.ep_mount_ret_t_u.volume.clusters[i].cid = cluster->cid;
                ret.ep_mount_ret_t_u.volume.clusters[i].storages_nb = cluster->nb_ms;

                for (j = 0; j < cluster->nb_ms; j++) {
                    volume_storage_t *p = (cluster->ms) + j;
                    strcpy(ret.ep_mount_ret_t_u.volume.clusters[i].storages[j].host, p->host);
                    ret.ep_mount_ret_t_u.volume.clusters[i].storages[j].sid = p->sid;
                }
                i++;
            }
        }
    }

    ret.ep_mount_ret_t_u.volume.clusters_nb = i;

    if ((errno = pthread_rwlock_unlock(&volumes_list.lock)) != 0) {
        ret.status = EP_FAILURE;
        goto error;
    }

    ret.ep_mount_ret_t_u.volume.eid = *eid;
    memcpy(ret.ep_mount_ret_t_u.volume.md5, exp->md5, ROZOFS_MD5_SIZE);
    ret.ep_mount_ret_t_u.volume.rl = layout;
    memcpy(ret.ep_mount_ret_t_u.volume.rfid, exp->rfid, sizeof (fid_t));

    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mount_ret_t_u.error = errno;
out:
    return &ret;
}

/* Will do something one day !! */
ep_status_ret_t *ep_umount_1_svc(uint32_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    DEBUG_FUNCTION;

    ret.status = EP_SUCCESS;

    return &ret;
}

ep_statfs_ret_t *ep_statfs_1_svc(uint32_t * arg, struct svc_req * req) {
    static ep_statfs_ret_t ret;
    DEBUG_FUNCTION;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export((eid_t) * arg)))
        goto error;
    if (export_stat(exp, (estat_t *) & ret.ep_statfs_ret_t_u.stat) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_statfs_ret_t_u.error = errno;
out:
    return &ret;
}

ep_mattr_ret_t *ep_lookup_1_svc(ep_lookup_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_lookup
            (exp, arg->parent, arg->name,
            (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:
    return &ret;
}

ep_mattr_ret_t *ep_getattr_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_getattr
            (exp, arg->fid, (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:
    return &ret;
}

ep_mattr_ret_t *ep_setattr_1_svc(ep_setattr_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_setattr(exp, arg->attrs.fid, (mattr_t *) & arg->attrs) != 0)
        goto error;
    if (export_getattr
            (exp, arg->attrs.fid, (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:

    return &ret;
}

ep_readlink_ret_t *ep_readlink_1_svc(ep_mfile_arg_t * arg,
        struct svc_req * req) {
    static ep_readlink_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_readlink(exp, arg->fid, ret.ep_readlink_ret_t_u.link) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_readlink_ret_t_u.error = errno;
out:

    return &ret;
}

ep_mattr_ret_t *ep_mknod_1_svc(ep_mknod_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_mknod
            (exp, arg->parent, arg->name, arg->uid, arg->gid, arg->mode,
            (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:

    return &ret;
}

ep_mattr_ret_t *ep_mkdir_1_svc(ep_mkdir_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_mkdir
            (exp, arg->parent, arg->name, arg->uid, arg->gid, arg->mode,
            (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_unlink_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_unlink(exp, arg->fid) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_rmdir_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_rmdir(exp, arg->fid) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:

    return &ret;
}

ep_mattr_ret_t *ep_symlink_1_svc(ep_symlink_arg_t * arg, struct svc_req * req) {
    static ep_mattr_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;

    if (export_symlink(exp, arg->link, arg->parent, arg->name,
       (mattr_t *) & ret.ep_mattr_ret_t_u.attrs) != 0)
       goto error;

    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_mattr_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_rename_1_svc(ep_rename_arg_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_rename(exp, arg->from, arg->to_parent, arg->to_name) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:

    return &ret;
}

ep_readdir_ret_t *ep_readdir_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_readdir_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    xdr_free((xdrproc_t) xdr_ep_readdir_ret_t, (char *) &ret);
    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_readdir
            (exp, arg->fid, (child_t **) & ret.ep_readdir_ret_t_u.children) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_readdir_ret_t_u.error = errno;
out:

    return &ret;
}

ep_io_ret_t *ep_read_1_svc(ep_io_arg_t * arg, struct svc_req * req) {
    static ep_io_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if ((ret.ep_io_ret_t_u.length =
            export_read(exp, arg->fid, arg->offset, arg->length)) < 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_io_ret_t_u.error = errno;
out:

    return &ret;
}

ep_read_block_ret_t *ep_read_block_1_svc(ep_read_block_arg_t * arg,
        struct svc_req * req) {
    static ep_read_block_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    xdr_free((xdrproc_t) xdr_ep_read_block_ret_t, (char *) &ret);
    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    ret.ep_read_block_ret_t_u.dist.dist_len = arg->nrb;
    ret.ep_read_block_ret_t_u.dist.dist_val =
            xmalloc(arg->nrb * sizeof (dist_t));
    if (export_read_block
            (exp, arg->fid, arg->bid, arg->nrb,
            ret.ep_read_block_ret_t_u.dist.dist_val) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_read_block_ret_t_u.error = errno;
out:

    return &ret;
}

ep_io_ret_t *ep_write_1_svc(ep_io_arg_t * arg, struct svc_req * req) {
    static ep_io_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if ((ret.ep_io_ret_t_u.length =
            export_write(exp, arg->fid, arg->offset, arg->length)) < 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_io_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_write_block_1_svc(ep_write_block_arg_t * arg,
        struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;
    if (export_write_block(exp, arg->fid, arg->bid, arg->nrb, arg->dist) != 0)
        goto error;
    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_open_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;

    if (export_open(exp, arg->fid) != 0)
        goto error;

    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:

    return &ret;
}

ep_status_ret_t *ep_close_1_svc(ep_mfile_arg_t * arg, struct svc_req * req) {
    static ep_status_ret_t ret;
    export_t *exp;
    DEBUG_FUNCTION;

    if (!(exp = exports_lookup_export(arg->eid)))
        goto error;

    if (export_close(exp, arg->fid) != 0)
        goto error;

    ret.status = EP_SUCCESS;
    goto out;
error:
    ret.status = EP_FAILURE;
    ret.ep_status_ret_t_u.error = errno;
out:
    return &ret;
}
