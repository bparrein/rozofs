#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <limits.h>

int x;
int sz = 1; /* Default file size is 1 byte */
char *mbuffer;

void usage(void);
int validate(char *, int, char);

char thedir[PATH_MAX] = "."; /* Default is to use the current directory */

int cret;
int i;
int dirlen;

int test_file_create_simple(int nb_files) {
    int fd;
    int i = 0;
    char buf[100];

    printf("Create file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);

        fd = creat(buf, O_RDWR | 0600);
        if (fd < 0) {
            fprintf(stderr, "creat error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }
        close(fd);
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_access_simple(int nb_files) {
    int i, y;
    char buf[100];
    printf("Access file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);
        printf("File test (%s): ", buf);

        y = access(buf, W_OK | F_OK);

        if (y < 0) {
            fprintf(stderr, "access error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_stat_simple(int nb_files) {
    int i, y;
    char buf[100];
    struct stat mystat;

    printf("Stat file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);

        y = stat(buf, &mystat);


        if (y < 0) {
            fprintf(stderr, "stat error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }
        if (!S_ISREG(mystat.st_mode)) {
            fprintf(stderr, "file %s is not a regular file\n", buf);
            return -1;
        }
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_write_simple(int nb_files) {
    int i, len;
    int fd;
    char buf[100];
    char value;

    printf("Write file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);

        value = (char) ((i) & 0xff);
        memset(mbuffer, value, sz);

        if ((fd = open(buf, O_RDWR | O_CREAT, S_IFREG | S_IRUSR | S_IWUSR)) == -1) {
            fprintf(stderr, "open error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        if ((len = write(fd, mbuffer, sz)) == -1) {
            fprintf(stderr, "write error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        fsync(fd);
        close(fd);
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_write_symlink_simple(int nb_files) {
    int i, len;
    int fd;
    char buf[100];
    char value;

    printf("Write file test (with symlink): \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d_SL", i);

        value = (char) ((i + 1) & 0xff);
        memset(mbuffer, value, sz);

        if ((fd = open(buf, O_RDWR | O_CREAT, S_IFREG | S_IRUSR | S_IWUSR)) == -1) {
            fprintf(stderr, "open error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        if ((len = write(fd, mbuffer, sz)) == -1) {
            fprintf(stderr, "write error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        fsync(fd);
        close(fd);
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_read_simple(int nb_files) {
    int i, y, fd;
    char buf[100];
    char value;

    printf("Read file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);

        value = (char) ((i) &0xff);

        fd = open(buf, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "open error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        y = read(fd, mbuffer, sz);
        if (y < 0) {
            fprintf(stderr, "read error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        if (validate(mbuffer, sz, value) != 0) {
            fprintf(stderr, "validate read error for file %s\n", buf);
            close(fd);
            return -1;
        }
        close(fd);
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_read_symlink_simple(int nb_files) {
    int i, y, fd;
    char buf[100];
    char value;

    printf("Read file test (use symlink): \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d_SL", i);

        value = (char) ((i + 1) &0xff);

        fd = open(buf, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "open error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        y = read(fd, mbuffer, sz);
        if (y < 0) {
            fprintf(stderr, "read error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        if (validate(mbuffer, sz, value) != 0) {
            fprintf(stderr, "validate read error for file %s\n", buf);
            close(fd);
            return -1;
        }
        close(fd);
        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_rename_simple(int nb_files) {
    int i, y;
    char buf[100];
    char bufn[100];

    printf("Rename file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);
        sprintf(bufn, "test_file_%d_R", i);

        y = rename(buf, bufn);

        if (y < 0) {
            fprintf(stderr, "rename error for file %s to %s: %s\n", buf, bufn, strerror(errno));
            return -1;
        }
        printf("File test (%s to %s): OK\n", buf, bufn);

    }
    printf("\n");
    return 0;
}

int test_file_symlink_simple(int nb_files) {
    int i, y;
    char buf[100];
    char bufn[100];

    printf("Symlink file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);
        sprintf(bufn, "test_file_%d_SL", i);

        y = symlink(buf, bufn);

        if (y < 0) {
            fprintf(stderr, "symlink error for file %s to %s: %s\n", buf, bufn, strerror(errno));
            return -1;
        }
        printf("File test (%s point to %s): OK\n", bufn, buf);

    }
    printf("\n");
    return 0;
}

int test_file_chmod_simple(int nb_files) {
    int i, y;
    char buf[100];

    printf("Chmod file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d", i);

        y = chmod(buf, 0666);
        if (y < 0) {
            fprintf(stderr, "chmod error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }

        printf("File test (%s): OK\n", buf);
    }
    printf("\n");
    return 0;
}

int test_file_unlink_simple(int nb_files) {

    int i, y;
    char buf[100];
    char bufn[100];

    printf("Unlink file test: \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d_R", i);
        sprintf(bufn, "test_file_%d_SL", i);

        y = unlink(buf);

        if (y < 0) {
            fprintf(stderr, "unlink error for file %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("File test (%s): OK\n", buf);

        y = unlink(bufn);

        if (y < 0) {
            fprintf(stderr, "unlink error for file %s : %s\n", bufn, strerror(errno));
            return -1;
        }
        printf("File test (%s): OK\n", bufn);
    }
    printf("\n");
    return 0;
}

int test_file_unlink_non_existent_simple(int nb_files) {

    int i, y;
    char buf[100];

    printf("Unlink file test (non-existent): \n");

    for (i = 0; i < nb_files; i++) {

        sprintf(buf, "test_file_%d_NE", i);

        y = unlink(buf);

        if (y < 0) {
            fprintf(stderr, "(OK) unlink error for the non-existent file %s : %s\n\n", buf, strerror(errno));
            return 0;
        }
        printf("File test (%s): OK\n", buf);

    }
    fprintf(stderr, "unlink must be have an error for an non-existent file (%s)\n", buf);
    printf("\n");
    return -1;
}

int test_dir_create(int x) {
    int i, j, k;
    int ret;
    char buf[100];

    printf("Create directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);
        printf("%s", buf);

        // Create directory
        ret = mkdir(buf, 0777);
        if (ret < 0) {
            fprintf(stderr, "mkdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Create directory
            ret = mkdir(buf, 0777);
            if (ret < 0) {
                fprintf(stderr, "mkdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t\t");
            for (k = 0; k < x; k++) {

                sprintf(buf, "test_dir_%d_%d_%d", i, j, k);

                // Create directory
                ret = mkdir(buf, 0777);
                if (ret < 0) {
                    fprintf(stderr, "mkdir error for directory %s : %s\n", buf, strerror(errno));
                    return -1;
                }
                // Change directory
                if ((ret = chdir(buf)) != 0) {
                    fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                printf("%s (OK) ", buf);

                // Change directory
                if ((ret = chdir("..")) != 0) {
                    perror("Chdir failed");
                    return -1;
                }
            }
            printf("\n");
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_dir_traverse(int x) {
    int i, j, k, ret;
    char buf[100];

    printf("Traverse directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }
            printf("\t%s (OK)->\n", buf);

            printf("\t\t");
            for (k = 0; k < x; k++) {

                sprintf(buf, "test_dir_%d_%d_%d", i, j, k);

                // Change directory
                if ((ret = chdir(buf)) != 0) {
                    fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                printf("%s (OK) ", buf);

                // Change directory
                if ((ret = chdir("..")) != 0) {
                    fprintf(stderr, "chdir error : %s\n", strerror(errno));
                    return -1;
                }
            }
            printf("\n");
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_file_readdir(int x) {
    int i, j, k, ret1, ret;
    char buf[100];
    char buf_1[100];
    DIR *dirh;
    struct dirent *dirp;

    printf("Readdir test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            // Open the current directory
            dirh = opendir(".");
            if (dirh == 0) {
                fprintf(stderr, "opendir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);

            printf("\t\t");
            // Read the directory
            for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {

                if (strcmp(".", dirp->d_name) == 0 || strcmp("..", dirp->d_name) == 0) {
                    continue;
                }

                for (k = 0; k < x; k++) {

                    sprintf(buf, "test_dir_%d_%d_%d", i, j, k);
                    sprintf(buf_1, "test_file_%d_%d_%d", i, j, k);

                    if (strcmp(buf, dirp->d_name) == 0 || strcmp(buf_1, dirp->d_name) == 0) {
                        printf("%s ", dirp->d_name);
                    }
                }
            }

            ret1 = closedir(dirh);
            if (ret1 < 0) {
                fprintf(stderr, "closedir error: %s\n", strerror(errno));
                return -1;
            }

            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }

            printf("\n");
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_file_create_in_dir(int x) {
    int i, j, k, len;
    int fd;
    int ret;
    char buf[100];
    char value;

    printf("Create file (and write) in directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            perror("Chdir failed");
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);
            printf("\t\t");

            for (k = 0; k < x; k++) {

                sprintf(buf, "test_file_%d_%d_%d", i, j, k);

                value = (char) ((i^j^k) & 0xff);
                memset(mbuffer, value, sz);

                fd = creat(buf, O_RDWR | 0600);
                if (fd < 0) {
                    fprintf(stderr, "creat error for file %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                if ((len = write(fd, mbuffer, sz)) == -1) {
                    fprintf(stderr, "write error for file %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                fsync(fd);

                close(fd);

                printf("%s (OK) ", buf);

            }
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }
            printf("\n");
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_file_read_in_dir(int x) {
    int i, j, k, y, fd, ret;
    char buf[100];
    char value;

    printf("Read file in directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);

            for (k = 0; k < x; k++) {

                sprintf(buf, "test_file_%d_%d_%d", i, j, k);

                value = (char) ((i^j^k) &0xff);

                fd = open(buf, O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "open error for file %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                y = read(fd, mbuffer, sz);
                if (y < 0) {
                    fprintf(stderr, "read error for file %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                if (validate(mbuffer, sz, value) != 0) {
                    fprintf(stderr, "validate read error for file %s\n", buf);
                    close(fd);
                    return -1;
                }
                close(fd);
                printf("\t\t Read in %s (OK)\n", buf);

            }
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_file_unlink_in_dir(int x) {
    int i, j, k, y, ret;
    char buf[100];

    printf("Unlink file in directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);

            for (k = 0; k < x; k++) {

                sprintf(buf, "test_file_%d_%d_%d", i, j, k);

                y = unlink(buf);

                if (y < 0) {
                    fprintf(stderr, "unlink error for file %s : %s\n", buf, strerror(errno));
                    return -1;
                }

                printf("\t\t Unlink %s (OK)\n", buf);
            }
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }
        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }
    }
    printf("\n");
    return 0;
}

int test_unlink_all_dir(int x) {
    int i, j, k, y, ret;
    char buf[100];

    printf("Unlink all directories test: \n");

    for (i = 0; i < x; i++) {

        sprintf(buf, "dir_L1_%d", i);

        // Change directory
        if ((ret = chdir(buf)) != 0) {
            fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }
        printf("%s (OK)->\n", buf);

        for (j = 0; j < x; j++) {

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Change directory
            if ((ret = chdir(buf)) != 0) {
                fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }

            printf("\t%s (OK)->\n", buf);

            for (k = 0; k < x; k++) {

                sprintf(buf, "test_dir_%d_%d_%d", i, j, k);

                // Change directory
                if ((ret = chdir(buf)) != 0) {
                    fprintf(stderr, "chdir error for directory %s : %s\n", buf, strerror(errno));
                    return -1;
                }
                // Change directory
                if ((ret = chdir("..")) != 0) {
                    fprintf(stderr, "chdir error : %s\n", strerror(errno));
                    return -1;
                }
                // Remove directory
                if ((ret = rmdir(buf)) != 0) {
                    perror("Rmdir failed");
                    return -1;
                }

                printf("\t\t Rmdir %s (OK)\n", buf);
            }
            // Change directory
            if ((ret = chdir("..")) != 0) {
                fprintf(stderr, "chdir error : %s\n", strerror(errno));
                return -1;
            }

            sprintf(buf, "dir_L1_%d_L2_%d", i, j);

            // Remove directory
            if ((ret = rmdir(buf)) != 0) {
                fprintf(stderr, "rmdir error for directory %s : %s\n", buf, strerror(errno));
                return -1;
            }
            printf("\t Rmdir %s (OK)\n", buf);

        }
        // Change directory
        if ((ret = chdir("..")) != 0) {
            fprintf(stderr, "chdir error : %s\n", strerror(errno));
            return -1;
        }

        sprintf(buf, "dir_L1_%d", i);

        // Remove directory
        if ((ret = rmdir(buf)) != 0) {
            fprintf(stderr, "rmdir error for directory %s : %s\n", buf, strerror(errno));
            return -1;
        }

        printf("Rmdir %s (OK)\n", buf);
    }
    printf("\n");
    return 0;
}

void usage(void) {
    printf("     fileop [-f X ] [-s Y] [-d <dir>] [-h]\n");
    printf("\n");
    printf("     -f #      Force factor. X^3 files will be created and removed.\n");
    printf("     -s #      Optional. Sets filesize for the create/write. May use suffix 'K' or 'M'.\n");
    printf("     -d <dir>  Specify starting directory.\n");
    printf("     -h        Help text.\n");
    printf("\n");
    printf("     The structure of the file tree is:\n");
    printf("     X number of Level 1 directories, with X number of\n");
    printf("     level 2 directories, with X number of files in each\n");
    printf("     of the level 2 directories.\n");
    printf("\n");
    printf("     Example:  fileop 2\n");
    printf("\n");
    printf("             dir_1                        dir_2\n");
    printf("            /     \\                      /     \\ \n");
    printf("      sdir_1       sdir_2          sdir_1       sdir_2\n");
    printf("      /     \\     /     \\          /     \\      /     \\ \n");
    printf("   file_1 file_2 file_1 file_2   file_1 file_2 file_1 file_2\n");
    printf("\n");
    printf("   Each file will be created, and then Y bytes is written to the file.\n");
    printf("\n");
}

int validate(char *buffer, int size, char value) {
    register int i;
    register char *cp;
    register int size1;
    register char v1;
    v1 = value;
    cp = buffer;
    size1 = size;
    for (i = 0; i < size; i++) {
        if (*cp++ != v1)
            return (1);
    }
    return (0);
}

int main(int argc, char **argv) {
    if (argc == 1) {
        usage();
        exit(1);
    }
    int ret = 0;
    while ((cret = getopt(argc, argv, "hf:s:d: ")) != EOF) {
        switch (cret) {
            case 'h':
                usage();
                exit(0);
                break;
            case 'd':
                dirlen = strlen(optarg);
                if (optarg[dirlen - 1] == '/')
                    --dirlen;
                strncpy(thedir, optarg, dirlen);
                thedir[dirlen] = 0;
                break;
            case 'f': /* Force factor */
                x = atoi(optarg);
                if (x < 0)
                    x = 1;
                break;
            case 's': /* Size of files */
                sz = atoi(optarg);
                if (optarg[strlen(optarg) - 1] == 'k' ||
                        optarg[strlen(optarg) - 1] == 'K') {
                    sz = (1024 * atoi(optarg));
                }
                if (optarg[strlen(optarg) - 1] == 'm' ||
                        optarg[strlen(optarg) - 1] == 'M') {
                    sz = (1024 * 1024 * atoi(optarg));
                }
                if (sz < 0)
                    sz = 1;
                break;
        }
    }

    mbuffer = (char *) malloc(sz);
    memset(mbuffer, 'a', sz);

    /* change starting point */
    if ((ret = chdir(thedir)) != 0) {
        fprintf(stderr, "chdir error for directory %s : %s\n", thedir, strerror(errno));
        return -1;
    }
    if (x == 0)
        x = 1;

    printf("\nThis test working in %s, File size : %d bytes\n\n", thedir, sz);

    // Tests only for files
    if (test_file_create_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_access_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_stat_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_chmod_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_write_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_read_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_symlink_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_write_symlink_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_read_symlink_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_rename_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_unlink_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_unlink_non_existent_simple(x) != 0) {
        exit(EXIT_FAILURE);
    }
    // Tests for files AND directories
    if (test_dir_create(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_dir_traverse(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_create_in_dir(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_readdir(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_read_in_dir(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_file_unlink_in_dir(x) != 0) {
        exit(EXIT_FAILURE);
    }
    if (test_unlink_all_dir(x) != 0) {
        exit(EXIT_FAILURE);
    }

    printf("\nALL TESTS ARE OK\n");

    return (0);
}