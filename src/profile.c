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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
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

    FILE *file;

    // Open the output file
    if ((file = fopen(file_name, "rw")) < 0) {
        perror("open failed");
        return -1;
    }

    fprintf(file, "time spent in export exchanges: %ld sec, %ld usec\n",
            profile_time[1].tv_sec, profile_time[1].tv_usec);
    fprintf(file, "time spent in storage exchanges: %ld sec, %ld usec\n",
            profile_time[0].tv_sec, profile_time[0].tv_usec);
    fprintf(file, "time spent in transform forward: %ld sec, %ld usec\n",
            profile_time[2].tv_sec, profile_time[2].tv_usec);
    fprintf(file, "time spent in transform inverse: %ld sec, %ld usec\n",
            profile_time[3].tv_sec, profile_time[3].tv_usec);

    fclose(file);

    return 0;
}

void log_profile() {
    info("time spent in export exchanges: %ld sec, %ld usec\n",
         profile_time[1].tv_sec, profile_time[1].tv_usec);
    info("time spent in storage exchanges: %ld sec, %ld usec\n",
         profile_time[0].tv_sec, profile_time[0].tv_usec);
    info("time spent in transform forward: %ld sec, %ld usec\n",
         profile_time[2].tv_sec, profile_time[2].tv_usec);
    info("time spent in transform inverse: %ld sec, %ld usec\n",
         profile_time[3].tv_sec, profile_time[3].tv_usec);
}

void log_storage_profile() {
    info("time spent in managing incoming requests: %ld sec, %ld usec\n",
         profile_time[1].tv_sec, profile_time[1].tv_usec);
    info("time spent in execution of the requests: %ld sec, %ld usec\n",
         profile_time[0].tv_sec, profile_time[0].tv_usec);
    info("time spent in sending the result back: %ld sec, %ld usec\n",
         profile_time[2].tv_sec, profile_time[2].tv_usec);
}
