#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "xmalloc.h"
#include "transform.h"
#include <uuid/uuid.h>

#define BSIZE 8192              //BYTES
#define FORWARD 6
#define INVERSE	4

int timeval_subtract(struct timeval *result, struct timeval *t2,
                     struct timeval *t1) {
    long int diff =
        (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec +
                                                1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return (diff < 0);
}

void timeval_print(struct timeval *tv) {
    char buffer[30];
    time_t curtime;

    printf("%ld.%06ld", tv->tv_sec, tv->tv_usec);
    curtime = tv->tv_sec;
    strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
    printf(" = %s.%06ld\n", buffer, tv->tv_usec);
}

int main(int argc, char **argv) {
    pxl_t *support;
    projection_t *projections;
    int nrloop = 0, done;
    int mp;
    struct timeval tic;
    struct timeval toc;
    struct timeval elapse;

    if (argc < 2) {
        printf("%s : nr loop\n", argv[0]);
        return -1;
    }
    nrloop = atoi(argv[1]);
    support = xmalloc(BSIZE);
    memset(support, 1, BSIZE);
    projections = xmalloc(FORWARD * sizeof (projection_t));
    for (mp = 0; mp < FORWARD; mp++) {
        projections[mp].angle.p = mp - FORWARD / 2;
        projections[mp].angle.q = 1;
        projections[mp].size =
            abs(mp - FORWARD / 2) * (INVERSE - 1) +
            (BSIZE / sizeof (pxl_t) / INVERSE - 1) + 1;
        projections[mp].bins = xmalloc(projections[mp].size * sizeof (bin_t));
    }

    gettimeofday(&tic, NULL);
    for (done = 0; done < nrloop; done++)
        transform_forward(support, INVERSE, BSIZE / INVERSE / sizeof (pxl_t),
                          FORWARD, projections);
    gettimeofday(&toc, NULL);
    timeval_subtract(&elapse, &toc, &tic);
    printf("Forward: %ld.%06ld\n", elapse.tv_sec, elapse.tv_usec);

    gettimeofday(&tic, NULL);
    for (done = 0; done < nrloop; done++)
        transform_inverse(support, INVERSE, BSIZE / INVERSE / sizeof (pxl_t),
                          INVERSE, projections);
    gettimeofday(&toc, NULL);
    timeval_subtract(&elapse, &toc, &tic);
    printf("Inverse: %ld.%06ld\n", elapse.tv_sec, elapse.tv_usec);

    return 0;
}
