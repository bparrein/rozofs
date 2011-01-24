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
#include <sys/stat.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <limits.h>

#include <api.h>

int main(int argc, char *argv[]) {
    file_t *file;
    int fd;
    int len;
    int off;
    int nb_copy = 0;
    char * buffer;
    int buffer_size;
    char output_path[PATH_MAX];

    // Check args
    if (argc < 5) {
        printf("Usage: sendfile_multiple <host> <input file> <output file> <buffer size> <nb. of copy>\n");
        exit(1);
    }

    // Initialize the API
    if (api_initialize(argv[1]) != 0) {
        perror("api_initialize failed");
        exit(1);
    }

    // Get the buffer size
    if ((buffer_size = atoi(argv[4])) <= 0) {
        perror("buffer size failed");
        exit(1);
    }

    // Allocate buffer
    buffer = malloc(sizeof (char) * buffer_size);

    // Get the output path
    output_path[0] = '/';
    strncat(output_path, argv[3], sizeof (output_path) - 1);

    // Get the nb. of copy
    if ((nb_copy = atoi(argv[5])) <= 0) {
        perror("nb_copy failed");
        exit(1);
    }

    int i = 0;

    for (i = 1; i <= nb_copy; i++) {

        // Build the output filename
        char filename [ PATH_MAX ];
        sprintf(filename, "%s_%d", output_path, i);

        // Open the input file
        if ((fd = open(argv[2], O_RDONLY)) < 0) {
            perror("open failed");
            exit(1);
        }

        // Create the output file in the MDS
        if (api_mknod(filename, S_IFREG) != 0) {
            perror("api_mknod failed");
            exit(1);
        }

        // Open the output file
        if ((file = api_open(filename, O_RDWR)) == NULL) {
            perror("api_open failed");
            exit(1);
        }

        // Set the begin offset at 0
        off = 0;

        // Read the input file per block of buffer_size bytes
        while ((len = read(fd, buffer, buffer_size)) > 0) {

            // Write the output file per block of len bytes
            if (api_write(file, off, buffer, len) < 0) {
                perror("api_write failed");
                exit(1);
            }

            // Update the offset
            off += len;
        }

        // Close the input and output files
        api_close(file);
        close(fd);
    }

    // Free the buffer
    free(buffer);
    exit(0);
}

