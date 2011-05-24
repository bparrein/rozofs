#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "xmalloc.h"
#include "transform.h"
#include <fcntl.h>
#include <unistd.h>

#define BSIZE 8192              //BYTES
#define FORWARD 12
#define INVERSE	8

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
    pxl_t *support, *rec;
    projection_t *projections;
    int mp, in, r;
    if (argc < 2) {
        printf("%s : file\n", argv[0]);
        return -1;
    }
    if ((in = open(argv[1], O_RDONLY)) < 0) {
        perror("Can't open file.");
        return -1;
    }

    support = xmalloc(BSIZE);
    rec = xmalloc(BSIZE);
    projections = xmalloc(FORWARD * sizeof (projection_t));
    for (mp = 0; mp < FORWARD; mp++) {
        projections[mp].angle.p = mp - FORWARD / 2;
        projections[mp].angle.q = 1;
        projections[mp].size =
            abs(mp - FORWARD / 2) * (INVERSE - 1) +
            (BSIZE / sizeof (pxl_t) / INVERSE - 1) + 1;
        projections[mp].bins = xmalloc(projections[mp].size * sizeof (bin_t));
    }

    while ((r = read(in, support, BSIZE)) > 0) {
        transform_forward(support, INVERSE, BSIZE / INVERSE / sizeof (pxl_t),
                          FORWARD, projections);

        transform_inverse(rec, INVERSE, BSIZE / INVERSE / sizeof (pxl_t),
                          INVERSE, projections);
        fwrite(rec, sizeof (char), r, stdout);
    }

    return 0;
}
