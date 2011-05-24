#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "rozo.h"
#include "volume.h"
#include "xmalloc.h"

int main(int argc, char **argv) {
    int status = 0;
    int cluster_nb = 3;
    int storages_nb = 10;
    int j = 0;
    int i = 0;
    volume_stat_t vstat;

    static char *hostnames[] = { "server1", "server2", "server3", "server4",
        "server5", "server6", "server7", "server8", "server9", "server10"
    };

    // Initialize rozo
    rozo_initialize(LAYOUT_4_6_8);

    // Initialize volume
    if (volume_initialize() != 0) {
        fprintf(stderr, "Can't initialize volume: %s\n", strerror(errno));
        status = -1;
        goto out;
    }
    // Add clusters and storages
    for (i = 0; i < cluster_nb; i++) {

        volume_storage_t *storage =
            (volume_storage_t *) xmalloc(storages_nb *
                                         sizeof (volume_storage_t));

        for (j = 0; j < storages_nb; j++) {

            // Add storage to cluster
            if (volume_storage_initialize_capacity
                (storage + j, (i * storages_nb) + j, hostnames[j],
                 (i * 10) + j) != 0) {
                fprintf(stderr, "Can't add storage: %s\n", strerror(errno));
                status = -1;
                goto out;
            }
        }

        // Add cluster to volume
        if (volume_register(i, storage, storages_nb) != 0) {
            fprintf(stderr, "Can't add cluster to volume: %s\n",
                    strerror(errno));
            status = -1;
            goto out;
        }
    }

    // Print volume
    if (volume_print() != 0) {
        fprintf(stderr, "Can't print info about this volume: %s\n",
                strerror(errno));
        status = -1;
        goto out;
    }
    // Balance volume
    if (volume_balance() != 0) {
        fprintf(stderr, "Can't balance this volume: %s\n", strerror(errno));
        status = -1;
        goto out;
    }
    // Print volume
    if (volume_print() != 0) {
        fprintf(stderr, "Can't print info about this volume: %s\n",
                strerror(errno));
        status = -1;
        goto out;
    }

    uint16_t *sids = (uint16_t *) xmalloc(rozo_safe * sizeof (uint16_t));
    uint16_t *cid = (uint16_t *) xmalloc(sizeof (uint16_t));

    // Distribute volume
    if (volume_distribute(cid, sids) != 0) {
        fprintf(stderr, "Can't distibute volume %s\n", strerror(errno));
        status = -1;
        goto out;
    } else {

        printf("CID: %d\n", *cid);

        for (i = 0; i < rozo_safe; i++) {
            uint16_t *p = sids + i;
            printf("sid: %d\n", *p);
        }
    }

    // Stat volume
    if (volume_stat(&vstat) != 0) {
        fprintf(stderr, "Can't stat volume %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    printf("Volume bsize: %d\n", vstat.bsize);
    printf("Volume bfree: %lld\n", vstat.bfree);

    printf("Volume size: %d\n", volume_size());

    // Release volume
    if (volume_release() != 0) {
        fprintf(stderr, "Can't release volume %s\n", strerror(errno));
        status = -1;
        goto out;
    }

    free(sids);
    free(cid);

out:
    return status;
}
