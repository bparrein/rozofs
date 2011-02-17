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

#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <fuse.h>

#include "config.h"
#include "log.h"
#include "rozo_client.h"

// Should be in the context
// create a rozo_context struct
// and return it from init
// see init (fuse.h)
static char host[255];

static char export[255];

static rozo_client_t rozo_client;

// should return a context

/*
static void *rozofs_init(struct fuse_conn_info *conn) {

    DEBUG_FUNCTION;

    if (rozo_initialize() != 0) {
        severe("%s", strerror(errno));
        goto out;
    }

    if (rozo_client_initialize(&rozo_client, host, export) == -1) {
        severe("%s", strerror(errno));
        goto out;
    }

out:
    return NULL;
}
*/

static void rozofs_destroy(void *args) {

    DEBUG_FUNCTION;

    rozo_client_release(&rozo_client);
}

static int rozofs_statfs(const char *path, struct statvfs *st) {

    DEBUG_FUNCTION;

    if (rozo_client_statfs(&rozo_client, st) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_getattr(const char *path, struct stat *stbuf) {

    DEBUG_FUNCTION;

    if (rozo_client_stat(&rozo_client, path, stbuf) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

// XXX noops for now since time are set at server side

static int rozofs_utimens(const char *path, const struct timespec tv[2]) {

    DEBUG_FUNCTION;

    errno = 0;

    return -errno;
}

static int rozofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

    rozo_dirent_t *dirent;
    rozo_dirent_t *iterator;

    DEBUG_FUNCTION;

    if ((iterator = dirent = rozo_client_readdir(&rozo_client, path)) == NULL) {
        goto out;
    }

    while (iterator != NULL) {
        filler(buf, iterator->name, NULL, 0);
        iterator = iterator->next;
    }

    rozo_client_release_dirent(dirent);

    errno = 0;

out:
    return -errno;
}

static int rozofs_mknod(const char *path, mode_t mode, dev_t dev) {

    DEBUG_FUNCTION;

    if (rozo_client_mknod(&rozo_client, path, mode) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_mkdir(const char *path, mode_t mode) {

    DEBUG_FUNCTION;

    if (rozo_client_mkdir(&rozo_client, path, mode) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_rename(const char *from, const char *to) {

    DEBUG_FUNCTION;

    if (rozo_client_rename(&rozo_client, from, to) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_symlink(const char *target, const char *link) {

    DEBUG_FUNCTION;

    if (rozo_client_symlink(&rozo_client, target, link) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_readlink(const char *target, char *link, size_t bufsiz) {

    DEBUG_FUNCTION;

    if (rozo_client_readlink(&rozo_client, target, link) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_chmod(const char *path, mode_t mode) {

    DEBUG_FUNCTION;

    if (rozo_client_chmod(&rozo_client, path, mode) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_unlink(const char * path) {

    DEBUG_FUNCTION;

    if (rozo_client_unlink(&rozo_client, path) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_rmdir(const char * path) {

    DEBUG_FUNCTION;

    if (rozo_client_rmdir(&rozo_client, path) == -1) {
        goto out;
    }

    errno = 0;

out:
    return -errno;
}

static int rozofs_open(const char *path, struct fuse_file_info *info) {

    rozo_file_t *file;

    DEBUG_FUNCTION;

    // XXX manage mode
    if ((file = rozo_client_open(&rozo_client, path, S_IRWXU)) == NULL) {
        goto out;
    }

    info->fh = (unsigned long) file;

    errno = 0;
out:
    return -errno;
}

static int rozofs_release(const char *path, struct fuse_file_info *info) {

    DEBUG_FUNCTION;


    uint64_t fh = (info->fh);

    if (rozo_client_close((rozo_file_t *) ((unsigned int) info->fh)) != 0)
        return -errno;

    return 0;
}

static int rozofs_create(const char *path, mode_t mode, struct fuse_file_info *info) {

    int status;

    DEBUG_FUNCTION;

    if ((status = rozofs_mknod(path, mode, 0)) != 0) {
        return -errno;
    }

    if ((status = rozofs_open(path, info)) != 0) {
        return -errno;
    }

    return 0;
}

static int rozofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

    DEBUG_FUNCTION;

    size_t length = 0;

    if ((length = rozo_client_read(&rozo_client, (rozo_file_t *) (unsigned int) fi->fh, offset, buf, size)) != 0)
        goto out;
out:
    return length;
}

static int rozofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

    DEBUG_FUNCTION;

    size_t length = 0;

    if ((length = rozo_client_write(&rozo_client, (rozo_file_t *) (unsigned int) fi->fh, offset, buf, size)) == 0) {
        goto out;
    }

out:
    return length;
}

static int rozofs_access(const char *path, int i) {

    DEBUG_FUNCTION;

    errno = 0;

out:
    return -errno;
}

static int rozofs_truncate(const char *path, off_t off) {

    DEBUG_FUNCTION;

    if (rozo_client_truncate(&rozo_client, path, off) == -1)
        goto out;

    errno = 0;

out:
    return -errno;
}

static struct fuse_operations rozo_operations = {
    //.init = rozofs_init,
    .destroy = rozofs_destroy,
    .statfs = rozofs_statfs,
    .getattr = rozofs_getattr,
    .chmod = rozofs_chmod,
    .utimens = rozofs_utimens,
    .readdir = rozofs_readdir,
    .mknod = rozofs_mknod,
    //.create	= rozofs_create,
    .mkdir = rozofs_mkdir,
    .rename = rozofs_rename,
    .readlink = rozofs_readlink,
    .symlink = rozofs_symlink,
    .unlink = rozofs_unlink,
    .rmdir = rozofs_rmdir,
    .read = rozofs_read,
    .write = rozofs_write,
    .access = rozofs_access,
    .open = rozofs_open,
    .release = rozofs_release,
    .truncate = rozofs_truncate,
};

static void usage() {
    printf("Rozo fuse mounter - %s\n", VERSION);
    printf("Usage: rozofs {--help | -h} | {export_host export_path mount_point} \n\n");
    printf("\t-h, --help\tprint this message.\n");
    printf("\texport_host\taddress (or dns name) where exportd deamon is running.\n");
    printf("\texport_path\tname of an export see exportd.\n");
    printf("\tmount_point\tdirectory where filesystem will be mounted.\n");
    exit(EXIT_FAILURE);
};

int main(int argc, char *argv[]) {
    char c;
    char *fuse_argv[13];

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    while (1) {

        int option_index = 0;

        c = getopt_long(argc, argv, "h", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage();
                break;
            case '?':
                usage();
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (argc < 4) {
        usage();
    }

    strcpy(host, argv[1]);
    strcpy(export, argv[2]);

    fuse_argv[0] = argv[0];
    fuse_argv[1] = strdup("-s");
    fuse_argv[2] = strdup("-o");
    fuse_argv[3] = strdup("allow_other");
    fuse_argv[4] = strdup("-o");
    fuse_argv[5] = strdup("fsname=rozo");
    fuse_argv[6] = strdup("-o");
    fuse_argv[7] = strdup("subtype=rozo");
    // FUSE 2.8 REQUIERED
    fuse_argv[8] = strdup("-o");
    fuse_argv[9] = strdup("big_writes");
    fuse_argv[10] = strdup("-o");
    fuse_argv[11] = strdup("max_write=65536"); // Why ?
    fuse_argv[12] = argv[3];

    openlog("rozofs", LOG_PID, LOG_LOCAL0);

        if (rozo_initialize() != 0) {
        fprintf(stderr, "rozofs: failed to mount a rozo filesystem for export: %s from: %s on:"
                " %s\n%s\nSee log for more information\n", argv[2], argv[1], argv[3], strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (rozo_client_initialize(&rozo_client, host, export) == -1) {
        fprintf(stderr, "rozofs: failed to mount a rozo filesystem for export: %s from: %s on:"
                " %s\n%s\nSee log for more information\n", argv[2], argv[1], argv[3], strerror(errno));
        exit(EXIT_FAILURE);
    }

    info("mounting - export: %s from : %s on: %s", argv[2], argv[1], argv[3]);

    return fuse_main(13, fuse_argv, &rozo_operations, NULL);
}