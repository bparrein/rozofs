/*
 * profile.c
 *
 *  Created on: 21 mars 2011
 *      Author: Boris Lucas
 */

#include <fcntl.h>
#include "log.h"
#include "profile.h"

int timeval_subtract(result, x, y)
struct timeval *result, *x, *y;
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}



int timeval_addto(timeval * dest, timeval * toadd) {
    int usec_add = 0;
    if ((usec_add = dest->tv_usec + toadd->tv_usec) > 1000000) {
        dest->tv_sec += 1;
        usec_add -= 1000000;
    }

    dest->tv_sec += toadd->tv_sec;
    dest->tv_usec = usec_add;
    return 0;
}

int save_profile_result(char *file_name) {

    int fd;

    // Open the output file
    if ((fd =
         open(file_name, O_RDWR | O_CREAT,
              S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO)) < 0) {
        perror("open failed");
        return -1;
    }

    fprintf(fd, "time spent in export exchanges : %ld sec, %ld usec\n",
            profile_time[1].tv_sec, profile_time[1].tv_usec);
    fprintf(fd, "time spent in storage exchanges : %ld sec, %ld usec\n",
            profile_time[0].tv_sec, profile_time[0].tv_usec);
    fprintf(fd, "time spent in transform forward : %ld sec, %ld usec\n",
            profile_time[2].tv_sec, profile_time[2].tv_usec);
    fprintf(fd, "time spent in transform inverse : %ld sec, %ld usec\n",
            profile_time[3].tv_sec, profile_time[3].tv_usec);

    close(fd);

    return 0;
}

void log_profile() {
    info("time spent in export exchanges : %ld sec, %ld usec\n",
         profile_time[1].tv_sec, profile_time[1].tv_usec);
    info("time spent in storage exchanges : %ld sec, %ld usec\n",
         profile_time[0].tv_sec, profile_time[0].tv_usec);
    info("time spent in transform forward : %ld sec, %ld usec\n",
         profile_time[2].tv_sec, profile_time[2].tv_usec);
    info("time spent in transform inverse : %ld sec, %ld usec\n",
         profile_time[3].tv_sec, profile_time[3].tv_usec);
}


void log_storage_profile() {
    info("time spent in managing incoming requests : %ld sec, %ld usec\n",
         profile_time[1].tv_sec, profile_time[1].tv_usec);
    info("time spent in execution of the requests : %ld sec, %ld usec\n",
         profile_time[0].tv_sec, profile_time[0].tv_usec);
    info("time spent in sending the result back : %ld sec, %ld usec\n",
         profile_time[2].tv_sec, profile_time[2].tv_usec);
}
