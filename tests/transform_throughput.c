#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "transform.h"
#include "rozo.h"

#define BSIZE 8192

int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1) {
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
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
    char *support;
    projection_t *pforward;
    projection_t *pinverse;
    int mp;
    struct timeval tic;
    struct timeval toc;
    struct timeval elapse;

    rozo_initialize(8192, 16, 12, 8);

    support = malloc(BSIZE);
    memset(support, 1, BSIZE);
    pforward = malloc(ROZO_FORWARD * sizeof (projection_t));
    for (mp = 0; mp < ROZO_FORWARD; mp++) {
        pforward[mp].angle.p = rozo_angles[mp].p;
        pforward[mp].angle.q = rozo_angles[mp].q;
        pforward[mp].size = rozo_psizes[mp];
        pforward[mp].bins = malloc(rozo_psizes[mp] * sizeof (char));
    }

    gettimeofday(&tic, NULL);
    transform_forward(support, ROZO_INVERSE, ROZO_BSIZE / ROZO_INVERSE, ROZO_FORWARD, pforward);
    gettimeofday(&toc, NULL);
    timeval_subtract(&elapse, &toc, &tic);
    printf("%ld.%06ld\n", elapse.tv_sec, elapse.tv_usec);

    pinverse = malloc(ROZO_INVERSE * sizeof (projection_t));
    for (mp = 0; mp < ROZO_INVERSE; mp++) {
        pinverse[mp].angle.p = rozo_angles[mp+2].p;
        pinverse[mp].angle.q = rozo_angles[mp+2].q;
        pinverse[mp].size = rozo_psizes[mp+2];
        pinverse[mp].bins = malloc(rozo_psizes[mp+2] * sizeof (char));
        memcpy(pinverse[mp].bins, pforward[mp+2].bins, rozo_psizes[mp+2]);
    }

    gettimeofday(&tic, NULL);
    transform_inverse(support, ROZO_INVERSE, ROZO_BSIZE / ROZO_INVERSE, ROZO_INVERSE, pinverse);
    gettimeofday(&toc, NULL);
    timeval_subtract(&elapse, &toc, &tic);
    printf("%ld.%06ld\n", elapse.tv_sec, elapse.tv_usec);

    return 0;
}
