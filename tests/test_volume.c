#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include "rozofs.h"
#include "volume.h"
#include "xmalloc.h"

int main(int argc, char **argv) {

    int status = 0;
    /*
        int status = 0;
        int cluster_nb = 1;
        int storages_nb = 8;
        int j = 0;
        int i = 0;
        volume_stat_t vstat;

        static char *hostnames[] = { "server1", "server2", "server3", "server4",
            "server5", "server6", "server7", "server8", "server9", "server10"
        };

        // Initialize rozofs
        rozofs_initialize(LAYOUT_4_6_8);

        // Initialize volume
        if (volume_initialize() != 0) {
            fprintf(stderr, "Can't initialize volume: %s\n", strerror(errno));
            status = -1;
            goto out;
        }
        // Add clusters and storages
        for (i = 1; i <= cluster_nb; i++) {

            volume_storage_t *storage =
                (volume_storage_t *) xmalloc(storages_nb *
                                             sizeof (volume_storage_t));

            for (j = 0; j < storages_nb; j++) {

                // Add storage to cluster
                if (mstorage_initialize
                    (storage + j, (i * storages_nb) + j, hostnames[j]) != 0) {
                    fprintf(stderr, "Can't add storage: %s\n", strerror(errno));
                    status = -1;
                    goto out;
                }
                (storage + j)->stat.free = 10;
                (storage + j)->stat.size = 50;

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
     */
    // Balance volume
    /*
       if (volume_balance() != 0) {
       fprintf(stderr, "Can't balance this volume: %s\n", strerror(errno));
       status = -1;
       goto out;
       }
     */
    // Print volume
    /*
       if (volume_print() != 0) {
       fprintf(stderr, "Can't print info about this volume: %s\n",
       strerror(errno));
       status = -1;
       goto out;
       }
     */

    /*
        uint16_t *sids = (uint16_t *) xmalloc(rozofs_safe * sizeof (uint16_t));
        uint16_t *cid = (uint16_t *) xmalloc(sizeof (uint16_t));

        // Distribute volume
        if (volume_distribute(cid, sids) != 0) {
            fprintf(stderr, "Can't distibute volume %s\n", strerror(errno));
            status = -1;
            goto out;
        } else {

            printf("CID: %d\n", *cid);

            for (i = 0; i < rozofs_safe; i++) {
                uint16_t *p = sids + i;
                printf("sid: %d\n", *p);
            }
        }
     */

    /*
        // Stat volume
        (volume_stat(&vstat));

        printf("Volume bsize: %d\n", vstat.bsize);
        printf("Volume bfree: %" PRIu64 "\n", vstat.bfree);
        printf("Volume size: %d\n", volume_size());

        sid_t sid_look = 8;

        char host[ROZOFS_HOSTNAME_MAX];

        printf("lookup storage with SID %u: %s\n", sid_look,
               lookup_volume_storage(sid_look, host));

        // Release volume
        if (volume_release() != 0) {
            fprintf(stderr, "Can't release volume %s\n", strerror(errno));
            status = -1;
            goto out;
        }

        free(sids);
        free(cid);
     */

/*
out:
*/
    return status;
}
