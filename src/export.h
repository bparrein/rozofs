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

#ifndef _EXPORT_H
#define _EXPORT_H

#include <limits.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#include "rozo.h"
#include "htable.h"
#include "dist.h"

typedef struct export {
    eid_t eid;
    char root[PATH_MAX];        // absolute path.
    fid_t rfid;                 // root fid.
    list_t mfiles;
    htable_t hfids;             // fid indexed.
    htable_t h_pfids;           // parent fid indexed.
} export_t;

int export_create(const char *root);

int export_initialize(export_t * e, eid_t eid, const char *root);

void export_release(export_t * e);

int export_stat(export_t * e, estat_t * st);

int export_lookup(export_t * e, fid_t parent, const char *name,
                  mattr_t * attrs);

int export_getattr(export_t * e, fid_t fid, mattr_t * attrs);

int export_setattr(export_t * e, fid_t fid, mattr_t * attrs);

int export_readlink(export_t * e, fid_t fid, char link[PATH_MAX]);

int export_mknod(export_t * e, fid_t parent, const char *name, mode_t mode,
                 mattr_t * attrs);

int export_mkdir(export_t * e, fid_t parent, const char *name, mode_t mode,
                 mattr_t * attrs);

int export_unlink(export_t * e, fid_t fid);

int export_rmdir(export_t * e, fid_t fid);

int export_symlink(export_t * e, fid_t target, fid_t parent, const char *name,
                   mattr_t * attrs);

int export_rename(export_t * e, fid_t from, fid_t parent, const char *name);

int64_t export_read(export_t * e, fid_t fid, uint64_t off, uint32_t len);

int export_read_block(export_t * e, fid_t fid, bid_t bid, uint32_t n,
                      dist_t * d);

int64_t export_write(export_t * e, fid_t fid, uint64_t off, uint32_t len);

int export_write_block(export_t * e, fid_t fid, bid_t bid, uint32_t n,
                       dist_t d);

int export_readdir(export_t * e, fid_t fid, child_t ** children);

int export_open(export_t * e, fid_t fid);

int export_close(export_t * e, fid_t fid);

#endif
