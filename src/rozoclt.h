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

#ifndef _ROZOCLT_H
#define _ROZOCLT_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <uuid/uuid.h>


typedef struct rozo_file rozo_file_t;

typedef struct rozoclt {
    exportclt_t export;

} rozoclt_t;

rozo_file_t *rozo_client_open(rozo_client_t * rozo_client, const char *path,
                              mode_t mode);

int64_t rozo_client_read(rozo_client_t * rozo_client, rozo_file_t * file,
                         uint64_t off, char *buf, uint32_t len);

int64_t rozo_client_write(rozo_client_t * rozo_client, rozo_file_t * file,
                          uint64_t off, const char *buf, uint32_t len);

int rozo_client_close(rozo_file_t * file);

#endif
