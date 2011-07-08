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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "../tests/rpc_throughput.h"
#include "../src/rpcclt.h"

static char host[255];
static rpcclt_t rpcclt;

int main(int argc, char *argv[]) {
    int fd;
    uint32_t len;
    uint64_t off;
    char *buffer;
    int buffer_size;

    // Check args
    if (argc < 3) {
        fprintf(stderr,
                "Usage: rpc_throughput <host> <input file> <buffer size>\n");
        exit(EXIT_FAILURE);
    }

    strcpy(host, argv[1]);

    // Initialize the RPC client
    if (rpcclt_initialize
        (&rpcclt, host, RPC_THROUGHPUT_PROGRAM, RPC_THROUGHPUT_VERSION, 0,
         0) != 0) {
        perror("rpcclt_initialize failed");
        exit(EXIT_FAILURE);
    }
    // Get the buffer size
    if ((buffer_size = atoi(argv[3])) <= 0) {
        perror("buffer size failed");
        exit(EXIT_FAILURE);
    }
    // Allocate buffer
    buffer = malloc(sizeof (char) * buffer_size);

    // Open the input file
    if ((fd = open(argv[2], O_RDONLY)) < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    // Set the begin offset at 0
    off = 0;

    // Read the input file per block of buffer_size bytes
    while ((len = read(fd, buffer, buffer_size)) > 0) {

        rpc_th_status_ret_t *ret = 0;
        rpc_th_write_arg_t args;

        args.bins.bins_val = malloc(len * sizeof (char *));
        memcpy(args.bins.bins_val, buffer, len);
        args.bins.bins_len = len;

        ret = rpc_th_write_1(&args, rpcclt.client);

        if (ret == 0)
            fprintf(stderr,
                    "rpc_th_write_1 failed (no response from storage server: %s)",
                    host);
        if (ret->status != 0)
            fprintf(stderr,
                    "rpc_th_write_1 failed: storage write response failure (%s)",
                    strerror(errno));

        // Update the offset
        off += len;
    }
    // Close the input file
    close(fd);
    // Free the buffer
    free(buffer);
    exit(EXIT_SUCCESS);
}
