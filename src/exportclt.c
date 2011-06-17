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

#include <errno.h>
#include "rozo.h"
#include "log.h"
#include "xmalloc.h"
#include "eproto.h"
#include "exportclt.h"

int exportclt_initialize(exportclt_t * clt, const char *host, char *root, uint32_t bufsize, uint32_t retries) {
    int status = -1;
    ep_mount_ret_t *ret = 0;
    int i = 0;
    int j = 0;
    DEBUG_FUNCTION;

    strcpy(clt->host, host);

    clt->retries = retries;
    clt->bufsize = bufsize;

    if (rpcclt_initialize
            (&clt->rpcclt, host, EXPORT_PROGRAM, EXPORT_VERSION,
            ROZO_RPC_BUFFER_SIZE, ROZO_RPC_BUFFER_SIZE) != 0)
        goto out;

    ret = ep_mount_1(&root, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mount_ret_t_u.error;
        goto out;
    }

    clt->eid = ret->ep_mount_ret_t_u.volume.eid;
    clt->rl = ret->ep_mount_ret_t_u.volume.rl;
    memcpy(clt->rfid, ret->ep_mount_ret_t_u.volume.rfid, sizeof (fid_t));

    // Initialize the list of clusters
    list_init(&clt->mcs);

    // For each cluster
    for (i = 0; i < ret->ep_mount_ret_t_u.volume.clusters_nb; i++) {

        ep_cluster_t ep_cluster = ret->ep_mount_ret_t_u.volume.clusters[i];

        mcluster_t *cluster = (mcluster_t *) xmalloc(sizeof (mcluster_t));

        DEBUG("CLUSTER: %d", ep_cluster.cid);

        cluster->cid = ep_cluster.cid;
        cluster->nb_ms = ep_cluster.storages_nb;

        cluster->ms = xmalloc(ep_cluster.storages_nb * sizeof (storageclt_t));

        for (j = 0; j < ep_cluster.storages_nb; j++) {

            DEBUG("SID:%d, HOST:%s", ep_cluster.storages[j].sid, ep_cluster.storages[j].host);

            //Initialize the connection with the storage
            if (storageclt_initialize(&cluster->ms[j], ep_cluster.storages[j].host, ep_cluster.storages[j].sid) != 0) {
                fatal("failed to join: %s,  %s", ep_cluster.storages[j].host, strerror(errno));
                goto out;
            }

        }
        // Add to the list
        list_push_back(&clt->mcs, &cluster->list);
    }

    // Initialize rozo
    if (rozo_initialize(clt->rl) != 0) {
        fatal("can't initialise rozo %s", strerror(errno));
        goto out;
    }

    status = 0;
out:
    return status;
}

void exportclt_release(exportclt_t * clt) {
    list_t *p, *q;

    DEBUG_FUNCTION;

    list_for_each_forward_safe(p, q, &clt->mcs) {
        mcluster_t *entry = list_entry(p, mcluster_t, list);
        free(entry->ms);
        list_remove(p);
        free(entry);
    }

    rpcclt_release(&clt->rpcclt);
}

int exportclt_stat(exportclt_t * clt, estat_t * st) {
    int status = -1;
    ep_statfs_ret_t *ret = 0;
    DEBUG_FUNCTION;

    ret = ep_statfs_1(&clt->eid, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_statfs_ret_t_u.error;
        goto out;
    }
    memcpy(st, &ret->ep_statfs_ret_t_u.stat, sizeof (estat_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_statfs_ret_t, (char *) ret);
    return status;
}

int exportclt_lookup(exportclt_t * clt, fid_t parent, char *name,
        mattr_t * attrs) {
    int status = -1;
    ep_lookup_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.parent, parent, sizeof (uuid_t));
    arg.name = name;
    ret = ep_lookup_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_getattr(exportclt_t * clt, fid_t fid, mattr_t * attrs) {
    int status = -1;
    ep_mfile_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (uuid_t));
    ret = ep_getattr_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_setattr(exportclt_t * clt, fid_t fid, mattr_t * attrs) {
    int status = -1;
    ep_setattr_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(&arg.attrs, attrs, sizeof (mattr_t));
    memcpy(arg.attrs.fid, fid, sizeof (fid_t));
    ret = ep_setattr_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_readlink(exportclt_t * clt, fid_t fid, char link[PATH_MAX]) {
    int status = -1;
    ep_mfile_arg_t arg;
    ep_readlink_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(&arg.fid, fid, sizeof (uuid_t));
    ret = ep_readlink_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_readlink_ret_t_u.error;
        goto out;
    }
    strcpy(link, ret->ep_readlink_ret_t_u.link);
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_readlink_ret_t, (char *) ret);
    return status;
}

int exportclt_mknod(exportclt_t * clt, fid_t parent, char *name, mode_t mode,
        mattr_t * attrs) {
    int status = -1;
    ep_mknod_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.parent, parent, sizeof (uuid_t));
    arg.name = name;
    arg.mode = mode;
    ret = ep_mknod_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_mkdir(exportclt_t * clt, fid_t parent, char *name, mode_t mode,
        mattr_t * attrs) {
    int status = -1;
    ep_mkdir_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.parent, parent, sizeof (uuid_t));
    arg.name = name;
    arg.mode = mode;
    ret = ep_mkdir_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_unlink(exportclt_t * clt, fid_t fid) {
    int status = -1;
    ep_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (uuid_t));
    ret = ep_unlink_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_rmdir(exportclt_t * clt, fid_t fid) {
    int status = -1;
    ep_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (uuid_t));
    ret = ep_rmdir_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_symlink(exportclt_t * clt, fid_t target, fid_t parent,
        char *name, mattr_t * attrs) {
    int status = -1;
    ep_symlink_arg_t arg;
    ep_mattr_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.target, target, sizeof (fid_t));
    memcpy(arg.link_parent, parent, sizeof (fid_t));
    arg.link_name = name;
    ret = ep_symlink_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_mattr_ret_t_u.error;
        goto out;
    }
    memcpy(attrs, &ret->ep_mattr_ret_t_u.attrs, sizeof (mattr_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_mattr_ret_t, (char *) ret);
    return status;
}

int exportclt_rename(exportclt_t * clt, fid_t from, fid_t parent, char *name) {
    int status = -1;
    ep_rename_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.from, from, sizeof (fid_t));
    memcpy(arg.to_parent, parent, sizeof (fid_t));
    arg.to_name = name;
    ret = ep_rename_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int64_t exportclt_read(exportclt_t * clt, fid_t fid, uint64_t off,
        uint32_t len) {
    int64_t lenght = -1;
    ep_io_arg_t arg;
    ep_io_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));
    arg.offset = off;
    arg.length = len;
    ret = ep_read_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_io_ret_t_u.error;
        goto out;
    }
    lenght = ret->ep_io_ret_t_u.length;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_io_ret_t, (char *) ret);
    return lenght;
}

int exportclt_read_block(exportclt_t * clt, fid_t fid, bid_t bid, uint32_t n,
        dist_t * d) {
    int status = -1;
    ep_read_block_arg_t arg;
    ep_read_block_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));
    arg.bid = bid;
    arg.nrb = n;
    ret = ep_read_block_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_read_block_ret_t_u.error;
        goto out;
    }
    memcpy(d, ret->ep_read_block_ret_t_u.dist.dist_val, n * sizeof (dist_t));
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_read_block_ret_t, (char *) ret);
    return status;
}

int64_t exportclt_write(exportclt_t * clt, fid_t fid, uint64_t off,
        uint32_t len) {
    int64_t lenght = -1;
    ep_io_arg_t arg;
    ep_io_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));
    arg.offset = off;
    arg.length = len;
    ret = ep_write_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_io_ret_t_u.error;
        goto out;
    }
    lenght = ret->ep_io_ret_t_u.length;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_io_ret_t, (char *) ret);
    return lenght;
}

int exportclt_write_block(exportclt_t * clt, fid_t fid, bid_t bid, uint32_t n,
        dist_t d) {
    int status = -1;
    ep_write_block_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));
    arg.bid = bid;
    arg.nrb = n;
    //arg.dist.dist_len = n;
    //arg.dist.dist_val = d;
    arg.dist = d;
    ret = ep_write_block_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_readdir(exportclt_t * clt, fid_t fid, child_t ** children) {
    int status = -1;
    ep_mfile_arg_t arg;
    ep_readdir_ret_t *ret = 0;
    ep_children_t it1;
    child_t **it2;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));
    ret = ep_readdir_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_readdir_ret_t_u.error;
        goto out;
    }

    it2 = children;
    it1 = ret->ep_readdir_ret_t_u.children;
    while (it1 != NULL) {
        *it2 = xmalloc(sizeof (child_t));
        (*it2)->name = strdup(it1->name);
        it2 = &(*it2)->next;
        it1 = it1->next;
    }
    *it2 = NULL;
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_readdir_ret_t, (char *) ret);
    return status;
}

int exportclt_open(exportclt_t * clt, fid_t fid) {

    int status = -1;
    ep_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));

    ret = ep_open_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}

int exportclt_close(exportclt_t * clt, fid_t fid) {

    int status = -1;
    ep_mfile_arg_t arg;
    ep_status_ret_t *ret = 0;
    DEBUG_FUNCTION;

    arg.eid = clt->eid;
    memcpy(arg.fid, fid, sizeof (fid_t));

    ret = ep_close_1(&arg, clt->rpcclt.client);
    if (ret == 0) {
        errno = EPROTO;
        goto out;
    }
    if (ret->status == EP_FAILURE) {
        errno = ret->ep_status_ret_t_u.error;
        goto out;
    }
    status = 0;
out:
    if (ret)
        xdr_free((xdrproc_t) xdr_ep_status_ret_t, (char *) ret);
    return status;
}
