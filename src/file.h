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

#ifndef _FILE_H
#define _FILE_H

#include "rozofs.h"
#include "exportclt.h"
#include "storageclt.h"

typedef struct file {
    fid_t fid;
    mode_t mode;
    mattr_t attrs;
    exportclt_t *export;
    storageclt_t **storages;
    //char buffer[ROZOFS_BUF_SIZE];
    char *buffer;
    int buf_write_wait;
    int buf_read_wait;
    uint64_t buf_pos;
    uint64_t buf_from;
} file_t;

file_t *file_open(exportclt_t * e, fid_t fid, mode_t mode);

int64_t file_write(file_t * f, uint64_t off, const char *buf, uint32_t len);

int file_flush(file_t * f);

int64_t file_read(file_t * f, uint64_t off, char **buf, uint32_t len);

int file_close(exportclt_t * e, file_t * f);

#endif
