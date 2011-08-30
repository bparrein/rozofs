#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "rozo.h"
#include "xmalloc.h"
#include "transform.h"
#include "storage.h"

int main(int argc, char **argv) {
    storage_t st;
    sid_t sid = 0;
    sstat_t sst;
    fid_t fid;
    bin_t *bins;

    rozo_initialize(LAYOUT_2_3_4);
    storage_initialize(&st, sid, "/tmp");

    if (storage_stat(&st, &sst) != 0) {
        perror("failed to stat storage");
        exit(-1);
    }
    printf("size: %" PRIu64 ", free: %" PRIu64 "\n", sst.size, sst.free);

    uuid_generate(fid);
    // Write some bins (15 prj)
    bins = xmalloc(rozo_psizes[0] * 15);
    if (storage_write(&st, fid, 0, 10, 15, bins) != 0) {
        perror("failed to write bins");
        exit(-1);
    }

    if (storage_truncate(&st, fid, 0, 10) != 0) {
        perror("failed to truncate pfile");
        exit(-1);
    }

    if (storage_rm_file(&st, fid) != 0) {
        perror("failed to remove pfile");
        exit(-1);
    }

    storage_release(&st);
    exit(0);
}
