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
#include "file.h"
#include "log.h"
#include "xmalloc.h"
#include "sproto.h"
#include "profile.h"


static storageclt_t *lookup_mstorage(exportclt_t * e, cid_t cid, sid_t sid) {
    list_t *iterator;
    int i = 0;
    DEBUG_FUNCTION;

    list_for_each_forward(iterator, &e->mcs) {
        mcluster_t *entry = list_entry(iterator, mcluster_t, list);
        if (cid == entry->cid) {
            for (i = 0; i < entry->nb_ms; i++) {
                if (sid == entry->ms[i].sid)
                    return &entry->ms[i];
            }
        }
    }
    warning("lookup_mstorage failed : mstorage not found");
    errno = EINVAL;
    return NULL;
}

/*
static void file_disconnect(file_t * f) {
    int i;
    DEBUG_FUNCTION;

    // Free the export
    for (i = 0; i < rozo_safe; i++) {
        storageclt_release(f->storages[i]);
        f->storages[i]->rpcclt.client = 0;
    }
}
*/

static int file_connect(file_t * f) {
    int i, connected;
    DEBUG_FUNCTION;

    // Nb. of storage servers connected
    connected = 0;
    // Get the hostname for one sid et one cid
    for (i = 0; i < rozo_safe; i++) {

        if ((f->storages[i] =
             lookup_mstorage(f->export, f->attrs.cid, f->attrs.sids[i]))) {

            if (f->storages[i]->rpcclt.client != 0) {
                connected++;
            } else {
                if (storageclt_initialize
                    (f->storages[i], f->storages[i]->host,
                     f->storages[i]->sid) != 0) {
                    warning("failed to join: %s,  %s", f->storages[i]->host,
                            strerror(errno));
                } else {
                    connected++;
                }
            }
        }
    }

    // Not enough server storage connections to retrieve the file
    if (connected < rozo_forward) {
        errno = EIO;
        return -1;
    }

    return 0;
}

static int file_reconnect(file_t * f) {
    DEBUG_FUNCTION;
    //file_disconnect(f);
    return file_connect(f);
}

static int read_blocks(file_t * f, bid_t bid, uint32_t nmbs, char *data) {
    int status = -1, i, j;
    dist_t *dist;               // Pointer to memory area where the block distribution will be stored
    dist_t *dist_iterator;
    uint8_t mp;
    bin_t **bins;
    projection_t *projections;
    angle_t *angles;
    uint16_t *psizes;
    DEBUG_FUNCTION;

    bins = xcalloc(rozo_inverse, sizeof (bin_t *));
    projections = xmalloc(rozo_inverse * sizeof (projection_t));
    angles = xmalloc(rozo_inverse * sizeof (angle_t));
    psizes = xmalloc(rozo_inverse * sizeof (uint16_t));
    memset(data, 0, nmbs * ROZO_BSIZE);
    dist = xmalloc(nmbs * sizeof (dist_t));

    if (exportclt_read_block(f->export, f->fid, bid, nmbs, dist) != 0)
        goto out;

    /* Until we don't decode all data blocks (nmbs blocks) */
    i = 0;                      // Nb. of blocks decoded (at begin = 0)
    dist_iterator = dist;
    while (i < nmbs) {
        if (*dist_iterator == 0) {
            i++;
            dist_iterator++;
            continue;
        }

        /* We calculate the number blocks with identical distributions */
        uint32_t n = 1;
        while ((i + n) < nmbs && *dist_iterator == *(dist_iterator + 1)) {
            n++;
            dist_iterator++;
        }
        // I don't know if it 's possible
        if (i + n > nmbs)
            goto out;
        // Nb. of received requests (at begin=0)
        int connected = 0;
        // For each projection
        PROFILE_STORAGE_START 
		for (mp = 0; mp < rozo_forward; mp++) {
            int mps = 0;
            int j = 0;
            bin_t *b;
            // Find the host for projection mp
            for (mps = 0; mps < rozo_safe; mps++) {
                if (dist_is_set(*dist_iterator, mps) && j == mp) {
                    break;
                } else {        // Try with the next storage server
                    j += dist_is_set(*dist_iterator, mps);
                }
            }

            if (!f->storages[mps]->rpcclt.client)
                continue;

            b = xmalloc(n * rozo_psizes[mp] * sizeof (bin_t));
            if (storageclt_read(f->storages[mps], f->fid, mp, bid + i, n, b)
                != 0) {
                free(b);
                continue;
            }
            bins[connected] = b;
            angles[connected].p = rozo_angles[mp].p;
            angles[connected].q = rozo_angles[mp].q;
            psizes[connected] = rozo_psizes[mp];

            // Increment the number of received requests
            if (++connected == rozo_inverse)
                break;
        }
        PROFILE_STORAGE_STOP
        // Not enough server storage response to retrieve the file
        if (connected < rozo_inverse) {
            errno = EIO;
            goto out;
        }

        PROFILE_TRANSFORM_START
        // Proceed the inverse data transform for the n blocks.
        for (j = 0; j < n; j++) {
            // Fill the table of projections for the block j
            // For each meta-projection
            for (mp = 0; mp < rozo_inverse; mp++) {
                // It's really important to specify the angles and sizes here
                // because the data inverse function sorts the projections.
                projections[mp].angle.p = angles[mp].p;
                projections[mp].angle.q = angles[mp].q;
                projections[mp].size = psizes[mp];
                projections[mp].bins = bins[mp] + (psizes[mp] * j);
            }

            // Inverse data for the block j
            transform_inverse((pxl_t *) (data + (ROZO_BSIZE * (i + j))),
                              rozo_inverse,
                              ROZO_BSIZE / rozo_inverse / sizeof (pxl_t),
                              rozo_inverse, projections);
        }
        PROFILE_TRANSFORM_INV_STOP
        // Free the memory area where are stored the bins.
        for (mp = 0; mp < rozo_inverse; mp++) {
            if (bins[mp])
                free(bins[mp]);
            bins[mp] = 0;
        }
        // Increment the nb. of blocks decoded
        i += n;
        // Shift to the next distribution
        dist_iterator++;
    }
    // If everything is OK, the status is set to 0
    status = 0;
out:
    // Free the memory area where are stored the bins used by the inverse transform
    if (bins) {
        for (mp = 0; mp < rozo_inverse; mp++)
            if (bins[mp])
                free(bins[mp]);
        free(bins);
    }
    if (projections)
        free(projections);
    if (angles)
        free(angles);
    if (psizes)
        free(psizes);
    if (dist)
        free(dist);
    return status;
}

static int write_blocks(file_t * f, bid_t bid, uint32_t nmbs,
                        const char *data) {
    int status;
    projection_t *projections;  // Table of projections used to transform data
    bin_t **bins;
    angle_t *angles;
    uint16_t *psizes;
    dist_t dist = 0;            // Important
    uint16_t mp = 0;
    uint16_t ps = 0;
    uint32_t i = 0;
    DEBUG_FUNCTION;

    projections = xmalloc(rozo_forward * sizeof (projection_t));
    bins = xcalloc(rozo_forward, sizeof (bin_t *));
    angles = xmalloc(rozo_forward * sizeof (angle_t));
    psizes = xmalloc(rozo_forward * sizeof (uint16_t));

    // For each projection
    for (mp = 0; mp < rozo_forward; mp++) {
        bins[mp] = xmalloc(rozo_psizes[mp] * nmbs * sizeof (bin_t));
        projections[mp].angle.p = rozo_angles[mp].p;
        projections[mp].angle.q = rozo_angles[mp].q;
        projections[mp].size = rozo_psizes[mp];
    }

    PROFILE_TRANSFORM_START
    /* Transform the data */
    // For each block to send
    for (i = 0; i < nmbs; i++) {
        // seek bins for each projection
        for (mp = 0; mp < rozo_forward; mp++) {
            // Indicates the memory area where the transformed data must be stored
            projections[mp].bins = bins[mp] + (rozo_psizes[mp] * i);
        }
        // Apply the erasure code transform for the block i
        transform_forward((pxl_t *) (data + (i * ROZO_BSIZE)), rozo_inverse,
                          ROZO_BSIZE / rozo_inverse / sizeof (pxl_t),
                          rozo_forward, projections);
    }
    PROFILE_TRANSFORM_FRWD_STOP
    /* Send requests to the storage servers */
    // For each projection server
    mp = 0;
    PROFILE_STORAGE_START 
	for (ps = 0; ps < rozo_safe; ps++) {
        // Warning: the server can be disconnected
        // but f->storages[ps].rpcclt->client != NULL
        // the disconnection will be detected when the request will be sent
        if (!(f->storages[ps]->rpcclt.client))
            continue;

        if (storageclt_write(f->storages[ps], f->fid, mp, bid, nmbs, bins[mp])
            != 0)
            continue;

        free(bins[mp]);
        bins[mp] = NULL;

        dist_set_true(dist, ps);

        if (++mp == rozo_forward)
            break;
    }
    PROFILE_STORAGE_STOP
    // Not enough server storage connections to store the file
    if (mp < rozo_forward) {
        errno = EIO;
        goto out;
    }
    if (exportclt_write_block(f->export, f->fid, bid, nmbs, dist) != 0) {
        errno = EIO;
        goto out;
    }

    status = 0;
out:
    if (bins) {
        for (mp = 0; mp < rozo_inverse; mp++)
            if (bins[mp])
                free(bins[mp]);
        free(bins);
    }
    if (projections)
        free(projections);
    if (angles)
        free(angles);
    if (psizes)
        free(psizes);
    return status;
}

// XXX : in each while (read_blocks(...))
// it's possible to do the file_reconnect() indefinitely
// EXAMPLE : WE HAVE 16 (0->16) STORAGE SERVERS
// AND I HAVE STORE A FILE IN STORAGE SERVER 0 to 11
// STORAGE SERVERS 0, 1, 2, 3 ARE OFFLINE BUT
// STORAGE SERVERS 12, 13, 14, 15 ARE ONLINE
// THEN CONNECT_FILE() IS GOOD BUT IT'S IMPOSSIBLE TO RETRIEVE THE FILE

static int64_t read_buf(file_t * f, uint64_t off, char *buf, uint32_t len) {
    int64_t length;
    uint64_t first;
    uint16_t foffset;
    uint64_t last;
    uint16_t loffset;
    int retry = 0;
    DEBUG_FUNCTION;

    if ((length = exportclt_read(f->export, f->fid, off, len)) < 0)
        goto out;

    first = off / ROZO_BSIZE;
    foffset = off % ROZO_BSIZE;
    last =
        (off + length) / ROZO_BSIZE + ((off + length) % ROZO_BSIZE ==
                                       0 ? -1 : 0);
    loffset = (off + length) - last * ROZO_BSIZE;

    // if our read is one block only
    if (first == last) {
        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);
        retry = 0;
        while (read_blocks(f, first, 1, block) != 0 &&
               retry++ < f->export->retries) {
            if (file_reconnect(f) != 0) {
                length = -1;
                goto out;
            }
        }
        memcpy(buf, &block[foffset], length);
    } else {
        char *bufp;
        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);
        bufp = buf;
        if (foffset != 0) {
            retry = 0;
            while (read_blocks(f, first, 1, block) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
            memcpy(buf, &block[foffset], ROZO_BSIZE - foffset);
            first++;
            bufp += ROZO_BSIZE - foffset;
        }
        if (loffset != ROZO_BSIZE) {
            retry = 0;
            while (read_blocks(f, last, 1, block) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
            memcpy(bufp + ROZO_BSIZE * (last - first), block, loffset);
            last--;
        }
        // Read the others
        if ((last - first) + 1 != 0) {
            retry = 0;
            while (read_blocks(f, first, (last - first) + 1, bufp) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
        }
    }

out:
    return length;
}

static int64_t write_buf(file_t * f, uint64_t off, const char *buf,
                         uint32_t len) {
    int64_t length = -1;
    uint64_t first;
    uint16_t foffset;
    int fread;
    uint64_t last;
    uint16_t loffset;
    int lread;
    int retry = 0;

    if (exportclt_getattr(f->export, f->attrs.fid, &f->attrs) != 0)
        goto out;

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

    // Is it neccesary to read the first block ?
    if (first <= (f->attrs.size / ROZO_BSIZE) && foffset != 0)
        fread = 1;
    else
        fread = 0;

    // Is it neccesary to read the last block ?
    if (last < (f->attrs.size / ROZO_BSIZE) && loffset != ROZO_BSIZE)
        lread = 1;
    else
        lread = 0;

    // If we must write only one block
    if (first == last) {
        char block[ROZO_BSIZE];
        memset(block, 0, ROZO_BSIZE);

        // If it's neccesary to read this block (first == last)
        if (fread == 1 || lread == 1) {
            retry = 0;
            while (read_blocks(f, first, 1, block) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
        }
        memcpy(&block[foffset], buf, len);
        retry = 0;
        while (write_blocks(f, first, 1, block) != 0 &&
               retry++ < f->export->retries) {
            if (file_reconnect(f) != 0) {
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
                retry = 0;
                while (read_blocks(f, first, 1, block) != 0 &&
                       retry++ < f->export->retries) {
                    if (file_reconnect(f) != 0) {
                        length = -1;
                        goto out;
                    }
                }
            }
            memcpy(&block[foffset], buf, ROZO_BSIZE - foffset);
            retry = 0;
            while (write_blocks(f, first, 1, block) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
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
                retry = 0;
                while (read_blocks(f, last, 1, block) != 0 &&
                       retry++ < f->export->retries) {
                    if (file_reconnect(f) != 0) {
                        length = -1;
                        goto out;
                    }
                }
            }
            memcpy(block, bufp + ROZO_BSIZE * (last - first), loffset);
            retry = 0;
            while (write_blocks(f, last, 1, block) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
            last--;
        }
        // Write the other blocks
        if ((last - first) + 1 != 0) {
            retry = 0;
            while (write_blocks(f, first, (last - first) + 1, bufp) != 0 &&
                   retry++ < f->export->retries) {
                if (file_reconnect(f) != 0) {
                    length = -1;
                    goto out;
                }
            }
        }
    }

    if (exportclt_write(f->export, f->fid, off, len) == -1) {
        length = -1;
        goto out;
    }
out:
    return length;
}

file_t *file_open(exportclt_t * e, fid_t fid, mode_t mode) {
    file_t *f = 0;
    DEBUG_FUNCTION;

    f = xmalloc(sizeof (file_t));

    memcpy(f->fid, fid, sizeof (fid_t));
    f->storages = xmalloc(rozo_safe * sizeof (storageclt_t *));
    if (exportclt_getattr(e, fid, &f->attrs) != 0)
        goto error;

    f->buffer = xmalloc(e->bufsize * sizeof (char));

    // Open the file descriptor in the export server
    if (exportclt_open(e, fid) != 0)
        goto error;

    f->export = e;

    if (file_connect(f) != 0)
        goto error;

    f->buf_from = 0;
    f->buf_pos = 0;
    f->buf_write_wait = 0;
    f->buf_read_wait = 0;
    goto out;
error:
    if (f) {
        int xerrno = errno;
        free(f);
        errno = xerrno;
    }
    f = 0;
out:
    return f;
}

int64_t file_write(file_t * f, uint64_t off, const char *buf, uint32_t len) {
    int done = 1;
    int64_t len_write = 0;
    DEBUG_FUNCTION;

    while (done) {

        if (len > (f->export->bufsize - f->buf_pos) ||
            (off != (f->buf_from + f->buf_pos) && f->buf_write_wait != 0)) {

            if ((len_write =
                 write_buf(f, f->buf_from, f->buffer, f->buf_pos)) < 0) {
                goto out;
            }

            f->buf_from = 0;
            f->buf_pos = 0;
            f->buf_write_wait = 0;

        } else {

            memcpy(f->buffer + f->buf_pos, buf, len);

            if (f->buf_write_wait == 0) {
                f->buf_from = off;
                f->buf_write_wait = 1;
            }
            f->buf_pos += len;
            len_write = len;
            done = 0;
        }
    }

out:
    return len_write;
}

int file_flush(file_t * f) {
    int status = -1;
    int64_t length;
    DEBUG_FUNCTION;

    if (f->buf_write_wait != 0) {

        if ((length = write_buf(f, f->buf_from, f->buffer, f->buf_pos)) < 0)
            goto out;

        f->buf_from = 0;
        f->buf_pos = 0;
        f->buf_write_wait = 0;
    }
    status = 0;
out:
    return status;
}

int64_t file_read(file_t * f, uint64_t off, char **buf, uint32_t len) {
    int64_t len_rec = 0;
    int64_t length = 0;
    DEBUG_FUNCTION;

    if ((off < f->buf_from) || (off >= (f->buf_from + f->buf_pos)) ||
        (len > (f->buf_from + f->buf_pos - off))) {

        if ((len_rec = read_buf(f, off, f->buffer, f->export->bufsize)) <= 0) {
            length = len_rec;
            goto out;
        }

        length = (len_rec > len) ? len : len_rec;
        *buf = f->buffer;

        f->buf_from = off;
        f->buf_pos = len_rec;
        f->buf_read_wait = 1;
    } else {
        length =
            (len <=
             (f->buf_pos - (off - f->buf_from))) ? len : (f->buf_pos - (off -
                                                                        f->buf_from));
        *buf = f->buffer + (off - f->buf_from);
    }

out:
    return length;
}

int file_close(exportclt_t * e, file_t * f) {
    int status = -1;
    DEBUG_FUNCTION;

    if (f) {
        f->buf_from = 0;
        f->buf_pos = 0;
        f->buf_write_wait = 0;
        f->buf_read_wait = 0;

        // Close the file descriptor in the export server
        if (exportclt_close(e, f->fid) != 0)
            goto out;

        free(f->storages);
        free(f->buffer);
        free(f);

    }
    status = 0;
out:
    return status;
}
