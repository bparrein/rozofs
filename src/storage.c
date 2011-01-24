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
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include "rozo.h"
#include "log.h"
#include "list.h"
#include "storage.h"

static inline char * storage_map(storage_t *storage, uuid_t mf, uint8_t mp, char *path) {

    char str[37];

    DEBUG_FUNCTION;

    strcpy(path, *storage);
    uuid_unparse(mf, str);
    strcat(path, str);
    sprintf(str, "-%d", mp);
    strcat(path, str);
    strcat(path, ".bins");

    return path;
}

int storage_write(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb, uint32_t nmbs, const uint8_t *bins) {

    int status;
    char path[PATH_MAX];
    int fd;

    DEBUG_FUNCTION;

    if ((fd = open(storage_map(storage, mf, mp, path), O_RDWR | O_CREAT, S_IFREG | S_IRUSR | S_IWUSR)) == -1) {
        status = -1;
        severe("storage_write failed: open file %s failed: %s", storage_map(storage, mf, mp, path), strerror(errno));
        goto out;
    }

    if ((pwrite(fd, bins, nmbs * rozo_psizes[mp] * sizeof (uint8_t), (off_t) mb * (off_t) rozo_psizes[mp])) != (nmbs * rozo_psizes[mp] * sizeof (uint8_t))) {
        status = -1;
        severe("storage_write failed: pwrite in file %s failed: %s", storage_map(storage, mf, mp, path), strerror(errno));
        goto out;
    }
    status = 0;

out:
    if (fd != -1) close(fd);
    return status;
}

int storage_read(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb, uint32_t nmbs, uint8_t *bins) {

    int status;
    char path[PATH_MAX];
    int fd;

    DEBUG_FUNCTION;

    if ((fd = open(storage_map(storage, mf, mp, path), O_RDWR, S_IFREG | S_IRUSR | S_IWUSR)) == -1) {
        status = -1;
        severe("storage_read failed: open file %s failed: %s", storage_map(storage, mf, mp, path), strerror(errno));
        goto out;
    }

    // XXX : read may not be complete
    if ((pread(fd, bins, nmbs * rozo_psizes[mp] * sizeof (uint8_t), (off_t) mb * (off_t) rozo_psizes[mp])) != (nmbs * rozo_psizes[mp] * sizeof (uint8_t))) {
        status = -1;
        severe("storage_read failed: pread in file %s failed: %s", storage_map(storage, mf, mp, path), strerror(errno));
        goto out;
    }

    status = 0;

out:
    if (fd != -1) close(fd);
    return status;
}

int storage_truncate(storage_t *storage, uuid_t mf, uint8_t mp, uint64_t mb) {

    int status;
    char path[PATH_MAX];

    DEBUG_FUNCTION;

    if (truncate(storage_map(storage, mf, mp, path), (mb + 1) * rozo_psizes[mp]) != 0) {
        status = -1;
        goto out;
    }

    status = 0;

out:
    return status;
}

int storage_remove(storage_t *storage, uuid_t mf, uint8_t mp) {

    int status;
    char path[PATH_MAX];

    DEBUG_FUNCTION;

    if (unlink(storage_map(storage, mf, mp, path)) != 0) {
        status = -1;
        goto out;
    }

    status = 0;

out:
    return status;
}

int storage_stat(storage_t *storage, sstat_t *sstat) {

    int status;
    struct statfs st;

    DEBUG_FUNCTION;

    if (statfs(*storage, &st) == -1) {
        status = -1;
        goto out;
    }

    sstat->bsize = st.f_bsize;
    sstat->bfree = st.f_bfree;

    status = 0;

out:
    return status;
}

