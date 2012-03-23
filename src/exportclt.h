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


#ifndef _EXPORTCLT_H
#define _EXPORTCLT_H

#include "rozofs.h"
#include "rpcclt.h"
#include "dist.h"
#include "storageclt.h"

//typedef struct mstorage {
//    sid_t sid;
//    char host[ROZOFS_HOSTNAME_MAX];
//} mstorage_t;

typedef struct mcluster {
    cid_t cid;
    storageclt_t *ms;
    uint16_t nb_ms;
    list_t list;
} mcluster_t;

typedef struct exportclt {
    char host[ROZOFS_HOSTNAME_MAX];
    char *root;
    eid_t eid;
    list_t mcs;
    rozofs_layout_t rl;
    fid_t rfid;
    uint32_t bufsize;
    uint32_t retries;
    rpcclt_t rpcclt;
} exportclt_t;

int exportclt_initialize(exportclt_t * clt, const char *host, char *root,
                         uint32_t bufsize, uint32_t retries);

int exportclt_reload(exportclt_t * clt);

void exportclt_release(exportclt_t * clt);

int exportclt_stat(exportclt_t * clt, estat_t * st);

int exportclt_lookup(exportclt_t * clt, fid_t parent, char *name,
                     mattr_t * attrs);

int exportclt_getattr(exportclt_t * clt, fid_t fid, mattr_t * attrs);

int exportclt_setattr(exportclt_t * clt, fid_t fid, mattr_t * attrs);

int exportclt_readlink(exportclt_t * clt, fid_t fid, char link[PATH_MAX]);

int exportclt_mknod(exportclt_t * clt, fid_t parent, char *name, uint32_t uid,
                    uint32_t gid, mode_t mode, mattr_t * attrs);

int exportclt_mkdir(exportclt_t * clt, fid_t parent, char *name, uint32_t uid,
                    uint32_t gid, mode_t mode, mattr_t * attrs);

int exportclt_unlink(exportclt_t * clt, fid_t fid);

int exportclt_rmdir(exportclt_t * clt, fid_t fid);

int exportclt_symlink(exportclt_t * clt, char *link, fid_t parent, char *name,
                      mattr_t * attrs);

int exportclt_rename(exportclt_t * clt, fid_t from, fid_t parent, char *name);

int64_t exportclt_read(exportclt_t * clt, fid_t fid, uint64_t off,
                       uint32_t len);

int exportclt_read_block(exportclt_t * clt, fid_t fid, bid_t bid, uint32_t n,
                         dist_t * d);

int64_t exportclt_write(exportclt_t * clt, fid_t fid, uint64_t off,
                        uint32_t len);

int exportclt_write_block(exportclt_t * clt, fid_t fid, bid_t bid, uint32_t n,
                          dist_t d);

int exportclt_readdir(exportclt_t * clt, fid_t fid, child_t ** children);

int exportclt_open(exportclt_t * clt, fid_t fid);

int exportclt_close(exportclt_t * clt, fid_t fid);

#endif
