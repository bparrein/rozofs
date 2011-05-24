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

#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log.h"
#include "pool.h"
#include "exportclt.h"
#include "rozo_client.h"

int rozo_client_initialize(rozo_client_t * rozo_client, const char *host,
                           char *export) {

    int status = -1;
    export_lookup_response_t *response = NULL;
    exportclt_t exp;

    DEBUG_FUNCTION;

    exportclt_initialize(&exp, "localhost", "/toto");

    if (pool_initialize(&rozo_client->pool) != 0) {
        goto out;
    }

    if (rpcclt_initialize
        (&rozo_client->export_client, host, EXPORT_PROGRAM, EXPORT_VERSION,
         8192, 8192) != 0) {
        severe("rpc_client_initialize failed: server %s unreachable: %s",
               host, strerror(errno));
        goto out;
    }

    if ((response =
         exportproc_lookup_1(&export,
                             rozo_client->export_client.rpcclt)) == NULL) {
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_lookup_response_t_u.error;
        goto out;
    }

    uuid_copy(rozo_client->export_uuid,
              response->export_lookup_response_t_u.uuid);

    status = 0;
out:
    if (status != 0)
        rozo_client_release(rozo_client);
    return status;
}

void rozo_client_release(rozo_client_t * rozo_client) {

    DEBUG_FUNCTION;

    pool_release(&rozo_client->pool);
    rpcclt_release(&rozo_client->export_client);
}

int rozo_client_statfs(rozo_client_t * rozo_client, struct statvfs *st) {

    int status;
    export_statfs_response_t *response = NULL;

    DEBUG_FUNCTION;

    if ((response =
         exportproc_statfs_1(rozo_client->export_uuid,
                             rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_statfs_response_t_u.error;
        status = -1;
        goto out;
    }

    memset(st, 0, sizeof (struct statvfs));

    st->f_bsize = response->export_statfs_response_t_u.stat.bsize;
    st->f_frsize = response->export_statfs_response_t_u.stat.bsize;
    st->f_blocks = response->export_statfs_response_t_u.stat.blocks;
    st->f_bfree = response->export_statfs_response_t_u.stat.bfree;
    st->f_bavail = response->export_statfs_response_t_u.stat.bfree;
    st->f_files = response->export_statfs_response_t_u.stat.files;
    st->f_ffree = response->export_statfs_response_t_u.stat.ffree;
    st->f_favail = response->export_statfs_response_t_u.stat.ffree;
    st->f_namemax = response->export_statfs_response_t_u.stat.namemax;

    status = 0;

out:
    if (response != NULL) {
        xdr_free((xdrproc_t) xdr_export_statfs_response_t, (char *) response);
    }
    return status;
}

int rozo_client_stat(rozo_client_t * rozo_client, const char *path,
                     struct stat *st) {

    int status;
    export_path_args_t args;
    export_stat_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_stat_1(&args,
                           rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_stat_response_t_u.error;
        status = -1;
        goto out;
    }

    memset(st, 0, sizeof (struct stat));

    st->st_mode = response->export_stat_response_t_u.stat.mode;
    st->st_nlink = response->export_stat_response_t_u.stat.nlink;
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = response->export_stat_response_t_u.stat.atime;
    st->st_mtime = response->export_stat_response_t_u.stat.mtime;
    st->st_ctime = response->export_stat_response_t_u.stat.ctime;
    st->st_size = response->export_stat_response_t_u.stat.size;

    status = 0;

out:
    if (args.path != NULL) {
        free(args.path);
    }
    if (response != NULL) {
        xdr_free((xdrproc_t) xdr_export_stat_response_t, (char *) response);
    }
    return status;
}

int rozo_client_readlink(rozo_client_t * rozo_client, const char *target,
                         char *link) {

    int status;
    export_path_args_t args;
    export_readlink_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(target)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_readlink_1(&args,
                               rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_readlink_response_t_u.error;
        status = -1;
        goto out;
    }

    strcpy(link, response->export_readlink_response_t_u.path);

    status = 0;

out:
    if (args.path != NULL) {
        free(args.path);
    }
    if (response != NULL) {
        xdr_free((xdrproc_t) xdr_export_readlink_response_t,
                 (char *) response);
    }
    return status;
}

int rozo_client_mknod(rozo_client_t * rozo_client, const char *path,
                      mode_t mode) {

    int status;
    export_mknod_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;
    // DEBUG
    // mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    // DEBUG

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    args.mode = mode;

    if ((response =
         exportproc_mknod_1(&args,
                            rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

int rozo_client_mkdir(rozo_client_t * rozo_client, const char *path,
                      mode_t mode) {

    int status;
    export_mkdir_args_t args;
    export_status_response_t *response;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    args.mode = mode;

    if ((response =
         exportproc_mkdir_1(&args,
                            rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

rozo_dirent_t *rozo_client_readdir(rozo_client_t * rozo_client,
                                   const char *path) {

    int status;
    rozo_dirent_t *dirent = NULL;
    rozo_dirent_t **dirent_iterator;
    export_path_args_t args;
    export_list_response_t *response = NULL;
    export_children_t iterator;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_list_1(&args,
                           rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_list_response_t_u.error;
        status = -1;
        goto out;
    }

    dirent_iterator = &dirent;
    iterator = response->export_list_response_t_u.children;
    while (iterator != NULL) {
        // XXX in case of failure !!!
        *dirent_iterator = malloc(sizeof (rozo_dirent_t));
        (*dirent_iterator)->name = strdup(iterator->file);
        dirent_iterator = &((*dirent_iterator)->next);
        iterator = iterator->next;
    }
    *dirent_iterator = NULL;

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_list_response_t, (char *) response);
    return dirent;
}

int rozo_client_release_dirent(rozo_dirent_t * dirent) {

    rozo_dirent_t *iterator = dirent;
    while (iterator != NULL) {
        rozo_dirent_t *tmp = iterator->next;
        free(iterator->name);
        free(iterator);
        iterator = tmp;
    }

    return 0;
}

int rozo_client_unlink(rozo_client_t * rozo_client, const char *path) {

    int status, i;
    export_path_args_t args;
    export_status_response_t *export_status_response = NULL;
    export_attr_response_t *export_attr_response = NULL;
    struct stat st;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    if (rozo_client_stat(rozo_client, path, &st) != 0) {
        status = -1;
        goto out;
    }

    if (S_ISREG(st.st_mode)) {
        if ((export_attr_response =
             exportproc_attr_1(&args,
                               rozo_client->export_client.rpcclt)) == NULL) {
            status = -1;
            goto out;
        }

        if (export_attr_response->status == EXPORT_FAILURE) {
            status = -1;
            errno = export_attr_response->export_attr_response_t_u.error;
            goto out;
        }

        for (i = 0; i < ROZO_SAFE; i++) {
            storage_remove_args_t storage_remove_args;
            rpcclt_t client;

            uuid_copy(storage_remove_args.uuid,
                      export_attr_response->export_attr_response_t_u.attr.
                      uuids[i]);
            uuid_copy(storage_remove_args.mf,
                      export_attr_response->export_attr_response_t_u.
                      attr.uuid);
            if (rpcclt_initialize
                (&client,
                 export_attr_response->export_attr_response_t_u.attr.hosts[i],
                 STORAGE_PROGRAM, STORAGE_VERSION, 0, 0) == 0) {
                storageproc_remove_1(&storage_remove_args, client.client);
                rpcclt_release(&client);
            }
        }
    }

    if ((export_status_response =
         exportproc_unlink_1(&args,
                             rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (export_status_response->status == EXPORT_FAILURE) {
        errno = export_status_response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;
out:
    if (args.path != NULL)
        xdr_free((xdrproc_t) xdr_export_path_t, (char *) &args.path);
    if (export_status_response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t,
                 (char *) export_status_response);
    return status;
}

int rozo_client_rmdir(rozo_client_t * rozo_client, const char *path) {

    int status;
    export_path_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_rmdir_1(&args,
                            rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;
out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

int rozo_client_symlink(rozo_client_t * rozo_client, const char *target,
                        const char *link) {

    int status;
    export_symlink_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.target = strdup(target)) == NULL) {
        status = -1;
        goto out;
    }

    if ((args.link = strdup(link)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_symlink_1(&args,
                              rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;
out:
    if (args.target != NULL)
        free(args.target);
    if (args.link != NULL)
        free(args.link);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

int rozo_client_rename(rozo_client_t * rozo_client, const char *from,
                       const char *to) {

    int status;
    export_rename_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.from = strdup(from)) == NULL) {
        status = -1;
        goto out;
    }

    if ((args.to = strdup(to)) == NULL) {
        status = -1;
        goto out;
    }

    if ((response =
         exportproc_rename_1(&args,
                             rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;
out:
    if (args.from != NULL)
        free(args.from);
    if (args.to != NULL)
        free(args.to);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

int rozo_client_chmod(rozo_client_t * rozo_client, const char *path,
                      mode_t mode) {

    int status;
    export_chmod_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }
    args.mode = mode;

    if ((response =
         exportproc_chmod_1(&args,
                            rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

int rozo_client_truncate(rozo_client_t * rozo_client, const char *path,
                         uint64_t offset) {

    int status;
    export_trunc_args_t args;
    export_status_response_t *response = NULL;

    DEBUG_FUNCTION;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        status = -1;
        goto out;
    }
    args.offset = offset;

    if ((response =
         exportproc_trunc_1(&args,
                            rozo_client->export_client.rpcclt)) == NULL) {
        status = -1;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_status_response_t_u.error;
        status = -1;
        goto out;
    }

    status = 0;

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t, (char *) response);
    return status;
}

struct rozo_file {
    char *path;
    mode_t mode;
    uuid_t uuid;
    uuid_t uuids[ROZO_SAFE];
    char hosts[ROZO_SAFE][ROZO_HOSTNAME_MAX];
    //rpcclt_t storages[ROZO_SAFE];
    rpcclt_t *storages[ROZO_SAFE];
};

/*
static int disconnect_file(rozo_file_t *file) {

    int i;

    DEBUG_FUNCTION;

    for (i = 0; i < ROZO_SAFE; i++) {
        rpcclt_release(&file->storages[i]);
    }

    return 0;
}
*/

static int connect_file(rozo_client_t * rozo_client, rozo_file_t * file) {

    int i, connected;

    DEBUG_FUNCTION;

    //disconnect_file(file);

    // Nb. of storage servers connected
    connected = 0;

    // We will try to establish a connection with each server projections used to store this file
    for (i = 0; i < ROZO_SAFE; i++) {

        /*
           // XXX : rpcclt_initialize() function is too long
           // RPC connection with a storage server
           if (rpcclt_initialize(&file->storages[i], file->hosts[i],
           STORAGE_PROGRAM, STORAGE_VERSION, 32768, 32768) != 0) {
           warning("rpcclt_initialize failed: storage server: %s unreachable, %s",
           file->hosts[i], strerror(errno));
           continue;
           }
           DEBUG("rpcclt_initialize: storage server %d :%s", i, file->hosts[i]);
         */
        if (!
            (file->storages[i] =
             pool_get(&rozo_client->pool, file->hosts[i])))
            continue;
        //Increment the nb. of storage servers connected
        connected++;
    }

    // Not enough server storage connections to retrieve the file
    // XXX if connected > ROZO_INVERSE we can read !!!!!!
    if (connected < ROZO_FORWARD) {
        errno = EIO;            // I/O error
        return -1;
    }

    return 0;
}

rozo_file_t *rozo_client_open(rozo_client_t * rozo_client, const char *path,
                              mode_t mode) {

    rozo_file_t *file = NULL;
    export_path_args_t args;
    export_attr_response_t *response = NULL;
    int i;

    DEBUG_FUNCTION;

    if ((file = malloc(sizeof (rozo_file_t))) == NULL) {
        goto out;
    }

    if ((file->path = strdup(path)) == NULL) {
        rozo_client_close(file);
        file = NULL;
        goto out;
    }

    file->mode = mode;

    uuid_copy(args.uuid, rozo_client->export_uuid);

    if ((args.path = strdup(path)) == NULL) {
        rozo_client_close(file);
        file = NULL;
        goto out;
    }

    if ((response =
         exportproc_attr_1(&args,
                           rozo_client->export_client.rpcclt)) == NULL) {
        rozo_client_close(file);
        file = NULL;
        goto out;
    }

    if (response->status == EXPORT_FAILURE) {
        errno = response->export_attr_response_t_u.error;
        rozo_client_close(file);
        file = NULL;
        goto out;
    }

    uuid_copy(file->uuid, response->export_attr_response_t_u.attr.uuid);
    for (i = 0; i < ROZO_SAFE; i++) {
        //file->storages[i].rpcclt = NULL;
        //file->storages[i]->rpcclt = NULL; //XXX why ?
        strcpy(file->hosts[i],
               response->export_attr_response_t_u.attr.hosts[i]);
        uuid_copy(file->uuids[i],
                  response->export_attr_response_t_u.attr.uuids[i]);
    }

    if (connect_file(rozo_client, file) != 0) {
        rozo_client_close(file);
        file = NULL;
        goto out;
    }

out:
    if (args.path != NULL)
        free(args.path);
    if (response != NULL)
        xdr_free((xdrproc_t) xdr_export_attr_response_t, (char *) response);
    return file;
}

static int read_blocks(rozo_client_t * rozo_client, rozo_file_t * file,
                       int64_t mb, uint32_t nmbs, char *data) {

    int status, i, j;
    export_read_block_args_t export_read_block_args;    // Request send to the export server
    export_read_block_response_t *export_read_block_response;   // Pointer to where the export response will be stored
    storage_read_args_t storage_read_args;      // Request send to storage servers
    storage_read_response_t *storage_read_response;     // Pointer to where the storage server response will be stored
    dist_t *distribution;       // Pointer to memory area where the block distribution will be stored
    uint8_t mp;
    char *bins[ROZO_INVERSE];   // Table of pointers to where are stored the received bins
    angle_t angles[ROZO_INVERSE];
    int16_t psizes[ROZO_INVERSE];

    DEBUG_FUNCTION;

    // Fill bins table with 0
    memset(bins, 0, ROZO_INVERSE * sizeof (char *));

    // Fill data buffer with 0 (it's neccassary for sparse file)
    memset(data, 0, nmbs * ROZO_BSIZE);

    DEBUG("read_blocks: (mb:%lld, nmbs:%d)", mb, nmbs);

    /*
       This function is made of 4 steps:
       STEP 1: Preparation and send a request to the export server to obtain the distribution of the nmbs data blocks

       Until we don't decode all data blocks (block : mb to block (mb + nmbs))
       STEP 2.1: We calculate the number n of blocks with identical distribution
       STEP 2.2: Preparation and send requests to storage servers to obtained ROZO_INVERSE projections for n blocks
       STEP 2.3: Proceed the inverse data transform for the n blocks
     */

    /* Prepare and send a request to the export server to obtain the distribution of data blocks */

    uuid_copy(export_read_block_args.uuid, rozo_client->export_uuid);   // Copy the export filesystem uuid
    if ((export_read_block_args.path = strdup(file->path)) == NULL) {   // Copy the path
        status = -1;
        goto out;
    }
    export_read_block_args.mb = mb;     // Copy the block number
    export_read_block_args.nmbs = nmbs; //Copy the nb. of metablocks

    // Send the request to the export server
    export_read_block_response =
        exportproc_read_block_1(&export_read_block_args,
                                rozo_client->export_client.rpcclt);
    // Awaiting response from export server during TIMEOUT s
    // See function rpcclt_initialize() in rpcclt.c for change the TIMEOUT

    // If no response from export server or a error is occured in the export server
    if (export_read_block_response == NULL ||
        export_read_block_response->status == EXPORT_FAILURE) {
        if (export_read_block_response != NULL) {
            severe
                ("read_blocks failed: export read block response failure (%s)",
                 strerror(export_read_block_response->
                          export_read_block_response_t_u.error));
        } else {
            // If no response from export server during TIMEOUT seconds
            warning
                ("read_blocks failed: export read block (no response from export server)");
        }
        errno = EIO;            // I/O error
        status = -1;
        goto out;
        // XXX : If no response from export server, don't try to connect the file with storage servers
    }
    // distribution pointed to response memory area where the block distributions will be stored
    distribution =
        export_read_block_response->
        export_read_block_response_t_u.distribution.distribution_val;

    /* Until we don't decode all data blocks (nmbs blocks) */

    i = 0;                      // Nb. of blocks decoded (at begin = 0)
    while (i < nmbs) {

        // If the distribution is empty, then the block is filled with 0 and go to the following block
        // A distribution can be empty in the case of sparse file
        //if (!memcmp(distribution, empty_distribution, ROZO_SAFE)) {
        if (*distribution == 0) {
            i++;                // Increment the number of block
            //distribution += ROZO_SAFE; // Shift to the next distribution
            distribution++;     // Shift to the next distribution
            continue;           // Exit loop
        }

        /* We calculate the number blocks with identical distributions */

        uint32_t n = 1;         // Nb. of identical distributions (blocks with the same distribution) (Set to 1 for begin)
        // If the currently distribution and the following are the same
        //while ((i + n) < nmbs && !memcmp(distribution, distribution + ROZO_SAFE, ROZO_SAFE)) {
        while ((i + n) < nmbs && *distribution == *(distribution + 1)) {
            n++;                // Increment the number of block with the same distribution
            //distribution += ROZO_SAFE; // Shift to the next distribution
            distribution++;     // Shift to the next distribution
        }

        DEBUG
            ("read_blocks: %d block(s) have same distribution (block %llu to block %llu)",
             n, mb + i, mb + i + n - 1);

        // I don't know if it 's possible
        if (i + n > nmbs)
            goto out;

        // Nb. of received requests (at begin=0)
        int connected = 0;

        /* Preparation and send requests to storage servers to obtained ROZO_INVERSE projections for n blocks */

        // For each meta-projection
        for (mp = 0; mp < ROZO_FORWARD; mp++) {

            // Find the nb. of storage server where the meta-projection mp is stored
            // I think It's can be more effective because we know that the storage server ROZO_SAFE
            // can only store the ROZO_FORWARD meta projection

            int mps = 0;        // Current storage server
            int j = 0;          // Current meta-projection
            // Find the host for projection mp
            for (mps = 0; mps < ROZO_SAFE; mps++) {
                // If the storage server nb: (mps) store a meta-projection and this meta-projection nb. is mp
                if (dist_is_set(*distribution, mps) && j == mp) {
                    // It's OK, The storage server where the meta-projection mp is stored is find
                    break;
                } else {        // Try with the next storage server
                    j += dist_is_set(*distribution, mps);       // Increment the current meta-projection if it's neccessary
                }
            }

            DEBUG
                ("read_blocks: (block %llu to block %llu) projection: %d is on index: %d",
                 mb + i, mb + i + n - 1, mp, mps);

            // If there is no established connection with the storage server mps then do not prepare request
            //if (file->storages[mps].rpcclt == NULL) {
            if (file->storages[mps]->client == NULL) {
                continue;
            }

            /* Prepare the request to send to the storage server mps */
            uuid_copy(storage_read_args.uuid, file->uuids[mps]);        // Copy the storage UUID
            uuid_copy(storage_read_args.mf, file->uuid);        // Copy the file UUID
            storage_read_args.mp = mp;  // Copy the meta-projection nb.
            storage_read_args.mb = mb + i;      // Copy the current metablock nb.
            storage_read_args.nmbs = n; // Nb. of metablocks (same distribution)

            // Send the request to the storage server mps
            //storage_read_response = storageproc_read_1(&storage_read_args, file->storages[mps].rpcclt);
            storage_read_response =
                storageproc_read_1(&storage_read_args,
                                   file->storages[mps]->client);
            // Awaiting response from server (storages[mps]) during TIMEOUT
            // See function rpcclt_initialize() in rpcclt.c for change the TIMEOUT

            // If no response from storage server or a error is occured in the storage server
            if (storage_read_response == NULL ||
                storage_read_response->status == STORAGE_FAILURE) {
                if (storage_read_response != NULL) {
                    severe
                        ("read_blocks failed: storage read response failure (%s)",
                         strerror(storage_read_response->
                                  storage_read_response_t_u.error));

                    xdr_free((xdrproc_t) xdr_storage_read_response_t,
                             (char *) &storage_read_response);
                } else {
                    // If no response from storage server during TIMEOUT seconds
                    warning
                        ("read_blocks failed: storage read failed (no response from storage server)");
                    // Release the RPC connection with this storage server
                    // to not send request unnecessarily long
                    pool_discard(&rozo_client->pool, file->hosts[mps]);
                }
                // Try to retrieve the next meta-projection
                continue;
            }
            // Memory allocation for the bins
            if ((bins[connected] =
                 malloc(storage_read_response->storage_read_response_t_u.bins.
                        bins_len)) == NULL) {
                status = -1;
                goto out;
            }
            // Copy received bins
            memcpy(bins[connected],
                   storage_read_response->storage_read_response_t_u.bins.
                   bins_val,
                   storage_read_response->storage_read_response_t_u.bins.
                   bins_len);
            // We must save pointers to angles and size of projection
            // because we can have connected != mp
            angles[connected].p = rozo_angles[mp].p;
            angles[connected].q = rozo_angles[mp].q;
            psizes[connected] = rozo_psizes[mp];

            // Free the memory area where the response is stored
            xdr_free((xdrproc_t) xdr_storage_read_response_t,
                     (char *) &storage_read_response);
            free(storage_read_response->storage_read_response_t_u.bins.
                 bins_val);

            // Increment the number of received requests (and stop the loop if enough request received)
            if (++connected == ROZO_INVERSE)
                break;
        }

        // Not enough server storage response to retrieve the file
        if (connected < ROZO_INVERSE) {
            severe
                ("read_blocks failed: error not enough response from storage servers for retrieve the blocks"
                 " %llu to %llu of file: %s (only %d/%d)", mb, mb + nmbs,
                 file->path, connected, ROZO_INVERSE);
            errno = EIO;        // I/O error
            status = -1;
            goto out;           // -> test to reconnect the file (in function rozo_client_write())
        }

        /* Proceed the inverse data transform for the n blocks */

        // Table of projections used for proceed the reverse transformation of data
        projection_t projections[ROZO_INVERSE];
        // For each block with the same distribution
        for (j = 0; j < n; j++) {

            // Fill the table of projections for the block j

            // For each meta-projection
            for (mp = 0; mp < ROZO_INVERSE; mp++) {

                // It's really important to specify the angles and sizes here because
                // the data inverse function sorts the table of projections during its proceeding.

                projections[mp].angle.p = angles[mp].p; // projections[mp].angle.p point to angle p for projecction mp
                projections[mp].angle.q = angles[mp].q; // projections[mp].angle.q point to angle q for projecction mp
                projections[mp].size = psizes[mp];      // projections[mp].size point to size of projection mp
                // projections[mp].bins point where is stored the bins for the projection mp of block j
                projections[mp].bins = bins[mp] + (projections[mp].size * j);
            }

            // Inverse data for the block j
            if (transform_inverse
                (data + (ROZO_BSIZE * (i + j)), ROZO_INVERSE,
                 ROZO_BSIZE / ROZO_INVERSE, ROZO_INVERSE, projections) != 0) {
                severe
                    ("read_blocks failed: inverse data failure for block %llu of file: %s",
                     (mb + i + j), file->path);
                errno = EIO;
                status = -1;
                goto out;
            }
        }

        // Free the memory area where are stored the bins used by the inverse transform
        for (mp = 0; mp < ROZO_INVERSE; mp++) {
            if (bins[mp] != NULL)
                free(bins[mp]);
            bins[mp] = NULL;
        }

        // Increment the nb. of blocks decoded
        i += n;

        // Shift to the next distribution
        //distribution += ROZO_SAFE;
        distribution++;
    }

    // If everything is OK, the status is set to 0
    status = 0;

out:
    // Free the memory area where are stored the bins used by the inverse transform
    for (mp = 0; mp < ROZO_INVERSE; mp++)
        if (bins[mp] != NULL)
            free(bins[mp]);
    // Free the memory area where the request send to storage server is stored
    xdr_free((xdrproc_t) xdr_storage_read_args_t,
             (char *) &storage_read_args);
    // Free the memory area where the request send to export server is stored
    xdr_free((xdrproc_t) xdr_export_read_block_args_t,
             (char *) &export_read_block_args);
    // Free the memory area where the response from the export server is stored
    if (export_read_block_response != NULL)
        xdr_free((xdrproc_t) xdr_export_read_block_response_t,
                 (char *) export_read_block_response);
    return status;              // Return the result
}

int64_t rozo_client_read(rozo_client_t * rozo_client, rozo_file_t * file,
                         uint64_t off, char *buf, uint32_t len) {

    export_io_args_t export_io_args;
    export_io_response_t *export_io_response;
    int64_t length;
    uint64_t first;
    uint16_t foffset;
    uint64_t last;
    uint16_t loffset;

    DEBUG_FUNCTION;

    uuid_copy(export_io_args.uuid, rozo_client->export_uuid);

    if ((export_io_args.path = strdup(file->path)) == NULL) {
        length = -1;
        goto out;
    }
    export_io_args.offset = off;
    export_io_args.length = len;

    export_io_response =
        exportproc_read_1(&export_io_args, rozo_client->export_client.rpcclt);

    if (export_io_response == NULL ||
        export_io_response->status == EXPORT_FAILURE) {
        length = -1;
        goto out;
    }

    length = export_io_response->export_io_response_t_u.length;

    first = off / ROZO_BSIZE;
    foffset = off % ROZO_BSIZE;
    last =
        (off + length) / ROZO_BSIZE + ((off + length) % ROZO_BSIZE ==
                                       0 ? -1 : 0);
    loffset = (off + length) - last * ROZO_BSIZE;

    DEBUG("rozo_client_read: %lld, first: (%lld, %d), last: (%lld, %d)",
          length, first, foffset, last, loffset);

    // if our read is one block only
    if (first == last) {
        DEBUG("first == last");
        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);
        while (read_blocks(rozo_client, file, first, 1, block) != 0) {
            if (connect_file(rozo_client, file) != 0) {
                length = -1;
                goto out;
            }
            // XXX : it's possible to do the connect_file() indefinitely
            // EXAMPLE : WE HAVE 16 (0->16) STORAGE SERVERS AND I HAVE STORE A FILE IN STORAGE SERVER 0 to 11
            // STORAGE SERVERS 0, 1, 2, 3 ARE OFFLINE BUT STORAGE SERVERS 12, 13, 14, 15 ARE ONLINE
            // THEN CONNECT_FILE() IS GOOD BUT IT'S IMPOSSIBLE TO RETRIEVE THE FILE
        }
        memcpy(buf, &block[foffset], length);
    } else {
        char *bufp;
        char block[ROZO_BSIZE];

        memset(block, 0, ROZO_BSIZE);

        bufp = buf;

        if (foffset != 0) {
            DEBUG("first not complete");
            while (read_blocks(rozo_client, file, first, 1, block) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
                // XXX : it's possible to do the connect_file() indefinitely
                // EXAMPLE : WE HAVE 16 (0->16) STORAGE SERVERS AND I HAVE STORE A FILE IN STORAGE SERVER 0 to 11
                // STORAGE SERVERS 0, 1, 2, 3 ARE OFFLINE BUT STORAGE SERVERS 12, 13, 14, 15 ARE ONLINE
                // THEN CONNECT_FILE() IS GOOD BUT IT'S IMPOSSIBLE TO RETRIEVE THE FILE
            }
            memcpy(buf, &block[foffset], ROZO_BSIZE - foffset);
            first++;
            bufp += ROZO_BSIZE - foffset;
        }

        if (loffset != ROZO_BSIZE) {
            DEBUG("last not complete");
            while (read_blocks(rozo_client, file, last, 1, block) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
                // XXX : it's possible to do the connect_file() indefinitely
                // EXAMPLE : WE HAVE 16 (0->16) STORAGE SERVERS AND I HAVE STORE A FILE IN STORAGE SERVER 0 to 11
                // STORAGE SERVERS 0, 1, 2, 3 ARE OFFLINE BUT STORAGE SERVERS 12, 13, 14, 15 ARE ONLINE
                // THEN CONNECT_FILE() IS GOOD BUT IT'S IMPOSSIBLE TO RETRIEVE THE FILE
            }
            memcpy(bufp + ROZO_BSIZE * (last - first), block, loffset);
            last--;
        }
        // Read the others
        if ((last - first) + 1 != 0) {
            while (read_blocks
                   (rozo_client, file, first, (last - first) + 1,
                    bufp) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
                // XXX : it's possible to do the connect_file() indefinitely
                // EXAMPLE : WE HAVE 16 (0->16) STORAGE SERVERS AND I HAVE STORE A FILE IN STORAGE SERVER 0 to 11
                // STORAGE SERVERS 0, 1, 2, 3 ARE OFFLINE BUT STORAGE SERVERS 12, 13, 14, 15 ARE ONLINE
                // THEN CONNECT_FILE() IS GOOD BUT IT'S IMPOSSIBLE TO RETRIEVE THE FILE

            }
        }
    }

out:
    xdr_free((xdrproc_t) xdr_export_io_args_t, (char *) &export_io_args);
    if (export_io_response != NULL)
        xdr_free((xdrproc_t) xdr_export_io_response_t,
                 (char *) export_io_response);
    return length;
}

static int write_blocks(rozo_client_t * rozo_client, rozo_file_t * file,
                        uint64_t mb, uint32_t nmbs, const char *data) {

    int status;
    projection_t projections[ROZO_FORWARD];     // Table of projections used to transform data
    storage_write_args_t storage_write_args[ROZO_FORWARD];      // Table of request to send to storage servers
    export_write_block_args_t export_write_block_args;  // Request send to the export server
    export_status_response_t *export_response = NULL;   // Pointer to memory area where the export response will be stored
    uint16_t mp = 0;            // Index of projection
    uint16_t ps = 0;            // Index of storage server
    uint32_t i = 0;             // Index of data block

    DEBUG_FUNCTION;

    DEBUG("write_blocks: (mb:%lld, nmbs:%d)", mb, nmbs);

    /*
       This function is made of 4 steps:
       STEP 1: Preparation of the request to send to the export server
       STEP 2: Preparation of the requests to send to (ROZO_FORWARD) storage servers
       STEP 3: Transformation of the data to send and fill the requests with the transformed data
       STEP 4: Send requests to the storage servers
       STEP 5: Send request to the export server
     */

    /* Prepare the request to send to the export server */

    uuid_copy(export_write_block_args.uuid, rozo_client->export_uuid);  // Copy the export filesystem uuid
    if ((export_write_block_args.path = strdup(file->path)) == NULL) {  // Copy the path
        status = -1;
        goto out;
    }
    export_write_block_args.mb = mb;    // Copy the metablock number
    export_write_block_args.nmbs = nmbs;        // Copy the nb. of metablocks
    export_write_block_args.distribution = 0;

    // Prepare the requests to send to storage servers
    // We will try to prepare request for ROZO_FORWARD (of ROZO_SAFE)
    // storage servers used to store this file

    // For each projection
    for (mp = 0; mp < ROZO_FORWARD; mp++) {

        /* Preparation of the request to send to the storage server ps */

        uuid_copy(storage_write_args[mp].mf, file->uuid);       // Copy the file uuid
        // Copy the current projection number (the projection number = number of prepared requests)
        storage_write_args[mp].mp = mp;
        storage_write_args[mp].mb = mb; // Copy the metablock number
        storage_write_args[mp].nmbs = nmbs;     // Copy the nb. of metablocks
        // Nb. of bins to send (nb. of blocks * size of the projection connected)
        storage_write_args[mp].bins.bins_len =
            rozo_psizes[mp] * nmbs * sizeof (uint8_t);
        // Memory allocation for the bins
        storage_write_args[mp].bins.bins_val =
            malloc(rozo_psizes[mp] * nmbs * sizeof (uint8_t));
        // If memory allocation problem
        if (storage_write_args[mp].bins.bins_val == NULL) {
            status = -1;
            goto out;
        }
        projections[mp].angle.p = rozo_angles[mp].p;    // Copy the angle p for the projection connected
        projections[mp].angle.q = rozo_angles[mp].q;    // Copy the angle p for the projection connected
        projections[mp].size = rozo_psizes[mp]; // Copy the size of the projection connected
    }

    /* Transform the data to send and fill the requests with the transformed data */

    // For each block to send
    for (i = 0; i < nmbs; i++) {

        // For each projection
        for (mp = 0; mp < ROZO_FORWARD; mp++) {
            // Indicates the memory area where the transformed data must be stored
            projections[mp].bins =
                storage_write_args[mp].bins.bins_val +
                (rozo_psizes[mp] * i * sizeof (uint8_t));
        }

        // Apply the erasure code transform for the block i
        if (transform_forward
            (data + (i * ROZO_BSIZE), ROZO_INVERSE, ROZO_BSIZE / ROZO_INVERSE,
             ROZO_FORWARD, projections) != 0) {
            severe
                ("write_blocks failed: transform data failure for block %llu of file: %s",
                 (i + mb), file->path);
            errno = EIO;        // I/O error
            status = -1;
            goto out;           // -> test to reconnect the file (in function rozo_client_write())
        }
    }

    /* Send requests to the storage servers */

    // For each projection server
    mp = 0;
    for (ps = 0; ps < ROZO_SAFE; ps++) {

        // If there is no established connection with the storage server ps then do not send request
        // and try with the next server
        // Warning: the server can be disconnected but file->storages[ps].rpcclt != NULL
        // the disconnection will be detected when the request will be sent
        if (!file->storages[ps]->client)
            continue;

        uuid_copy(storage_write_args[mp].uuid, file->uuids[ps]);

        storage_status_response_t *response;    // Pointer to memory area where the response will be stored

        // Send the request to the storage server mp
        response =
            storageproc_write_1(&storage_write_args[mp],
                                file->storages[ps]->client);

        // Awaiting response from server (storages[mp]) during TIMEOUT
        // See function rpcclt_initialize() in rpcclt.c for change the TIMEOUT

        // If no response from storage server or a error is occured in the storage server
        if (response == NULL || response->status == STORAGE_FAILURE) {
            if (response != NULL) {
                severe
                    ("write_blocks failed: storage write response failure (%s)",
                     strerror(response->storage_status_response_t_u.error));
                // Free the memory area where the response is stored
                xdr_free((xdrproc_t) xdr_storage_status_response_t,
                         (char *) response);
                errno = EIO;    // I/O error
                status = -1;
                goto out;
            } else {
                // If no response from storage server during TIMEOUT seconds
                warning
                    ("write_blocks failed: storage write failed (no response from storage server: %s)",
                     file->hosts[ps]);
                pool_discard(&rozo_client->pool, file->hosts[ps]);
                continue;
            }
        }
        // If the request is sent, we set 1 in the distribution
        // i.e : The storage server ps store data for the block mb + nmbs
        //export_write_block_args.distribution[ps] = 1;
        dist_set_true(export_write_block_args.distribution, ps);

        // Free the memory area where the response is stored
        xdr_free((xdrproc_t) xdr_storage_status_response_t,
                 (char *) response);

        // Increment the number of send requests (and stop the loop if enough request prepared)
        if (++mp == ROZO_FORWARD)
            break;
    }

    // Not enough server storage connections to store the file
    if (mp < ROZO_FORWARD) {
        severe
            ("write_blocks failed: error not enough connections with storage servers for store the blocks"
             " %llu to %llu of file: %s (only %d/%d)", mb, mb + nmbs,
             file->path, mp, ROZO_FORWARD);
        errno = EIO;            // I/O error
        status = -1;
        goto out;               // -> test to reconnect the file (in function rozo_client_write())
    }
    // Send the request to the export server
    export_response =
        exportproc_write_block_1(&export_write_block_args,
                                 rozo_client->export_client.rpcclt);
    // Awaiting response from export server during TIMEOUT s
    // See function rpcclt_initialize() in rpcclt.c for change the TIMEOUT

    // If no response from export server or a error is occured in the export server
    if (export_response == NULL || export_response->status == EXPORT_FAILURE) {
        if (export_response != NULL) {
            severe
                ("write_blocks failed: export write block response failure (%s)",
                 strerror(export_response->export_status_response_t_u.error));
        } else {
            // If no response from export server during TIMEOUT seconds
            warning
                ("write_blocks failed: export write block (no response from export server)");
        }
        errno = EIO;            // I/O error
        status = -1;
        goto out;
        // XXX If no response from export server, don't try to connect the file with storage servers
    }
    // If everything is OK, the status is set to 0
    status = 0;
out:
    // Free the memory areas where the requests send to storage servers are stored
    for (mp = 0; mp < ROZO_FORWARD; mp++) {
        if (&storage_write_args[mp] != NULL) {
            xdr_free((xdrproc_t) xdr_storage_write_args_t,
                     (char *) &storage_write_args[mp]);
        }
    }
    // Free the memory area where the request send to export server is stored
    xdr_free((xdrproc_t) xdr_export_write_block_args_t,
             (char *) &export_write_block_args);
    // Free the memory area where the response from the export server is stored
    if (export_response != NULL)
        xdr_free((xdrproc_t) xdr_export_status_response_t,
                 (char *) export_response);
    return status;
}

// XXX assuming we will always find enough storages is fine unless a system error occurs,
// the while loops for writing block is infinite in case of system failure (e.g encode/decode error)
int64_t rozo_client_write(rozo_client_t * rozo_client, rozo_file_t * file,
                          uint64_t off, const char *buf, uint32_t len) {

    export_io_args_t export_io_args;
    export_io_response_t *export_io_response;
    int64_t length;
    struct stat st;
    uint64_t first;
    uint16_t foffset;
    int fread;
    uint64_t last;
    uint16_t loffset;
    int lread;

    if (rozo_client_stat(rozo_client, file->path, &st) != 0) {
        length = -1;
        goto out;
    }

    length = len;

    // Nb. of the first block to write
    first = off / ROZO_BSIZE;
    // Offset (in bytes) for the first block
    foffset = off % ROZO_BSIZE;
    // Nb. of the last block to write
    last =
        (off + length) / ROZO_BSIZE + ((off + length) % ROZO_BSIZE ==
                                       0 ? -1 : 0);
    // Offset (in bytes) for the last block
    loffset = (off + length) - last * ROZO_BSIZE;

    DEBUG("rozo_client_write: %lld, first: (%lld, %d), last: (%lld, %d)",
          length, first, foffset, last, loffset);

    // Is it neccesary to read the first block ?
    if (first <= (st.st_size / ROZO_BSIZE) && foffset != 0) {
        fread = 1;
    } else {
        fread = 0;
    }
    // Is it neccesary to read the last block ?
    if (last < (st.st_size / ROZO_BSIZE) && loffset != ROZO_BSIZE) {
        lread = 1;
    } else {
        lread = 0;
    }

    // If we must write only one block
    if (first == last) {

        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);

        // If it's neccesary to read this block (first == last)
        if (fread == 1 || lread == 1) {

            while (read_blocks(rozo_client, file, first, 1, block) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
            }
        }

        memcpy(&block[foffset], buf, len);

        while (write_blocks(rozo_client, file, first, 1, block) != 0) {
            if (connect_file(rozo_client, file) != 0) {
                length = -1;
                goto out;
            }
        }
    } else {                    // If we must write more than one block
        const char *bufp;
        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);

        bufp = buf;

        // Manage the first and last blocks if needed
        if (foffset != 0) {

            // If we need to read the first block
            if (fread == 1) {
                while (read_blocks(rozo_client, file, first, 1, block) != 0) {
                    if (connect_file(rozo_client, file) != 0) {
                        length = -1;
                        goto out;
                    }
                }
            }
            memcpy(&block[foffset], buf, ROZO_BSIZE - foffset);
            while (write_blocks(rozo_client, file, first, 1, block) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
            }
            first++;
            bufp += ROZO_BSIZE - foffset;
        }

        if (loffset != ROZO_BSIZE) {
            // If we need to read the last block
            if (lread == 1) {
                while (read_blocks(rozo_client, file, last, 1, block) != 0) {
                    if (connect_file(rozo_client, file) != 0) {
                        length = -1;
                        goto out;
                    }
                }
            }
            memcpy(block, bufp + ROZO_BSIZE * (last - first), loffset);
            while (write_blocks(rozo_client, file, last, 1, block) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
            }
            last--;
        }
        // Write the other blocks
        if ((last - first) + 1 != 0) {
            while (write_blocks
                   (rozo_client, file, first, (last - first) + 1,
                    bufp) != 0) {
                if (connect_file(rozo_client, file) != 0) {
                    length = -1;
                    goto out;
                }
            }
        }
    }

    // Update XATTR for the file size and for the nb. of blocks in mfs
    uuid_copy(export_io_args.uuid, rozo_client->export_uuid);

    if ((export_io_args.path = strdup(file->path)) == NULL) {
        length = -1;
        goto out;
    }
    export_io_args.offset = off;
    export_io_args.length = len;

    export_io_response =
        exportproc_write_1(&export_io_args,
                           rozo_client->export_client.rpcclt);

    if (export_io_response == NULL ||
        export_io_response->status == EXPORT_FAILURE) {
        length = -1;
        goto out;
    }

out:
    xdr_free((xdrproc_t) xdr_export_io_args_t, (char *) &export_io_args);
    if (export_io_response != NULL)
        xdr_free((xdrproc_t) xdr_export_io_response_t,
                 (char *) export_io_response);
    return length;
}

int rozo_client_close(rozo_file_t * file) {

    DEBUG_FUNCTION;

    if (file != NULL) {
        //disconnect_file(file);
        if (file->path != NULL) {
            free(file->path);
        }
        free(file);
        file = NULL;
    }
    return 0;
}
