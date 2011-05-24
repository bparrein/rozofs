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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "log.h"
#include "config.h"
#include "rozo.h"
#include "daemon.h"

/* manage only one daemon at a time */
static void (*daemon_on_stop) (void) = NULL;

static void (*daemon_on_usr1) (void) = NULL;

static int write_pid(const char *name) {
    int status = -1;
    int lfp = -1;
    char pidf[255] = DAEMON_PID_DIRECTORY;
    char str[10];
    DEBUG_FUNCTION;

    strcat(pidf, name);
    if ((lfp = open(pidf, O_RDWR | O_CREAT, 0640)) < 0)
        goto out;
    if (lockf(lfp, F_TLOCK, 0) < 0)
        goto out;
    sprintf(str, "%d\n", getpid());
    if (write(lfp, str, strlen(str)) != strlen(str))
        goto out;

    status = 0;
out:
    if (lfp > 0)
        close(lfp);
    return status;
}

static int read_pid(const char *name, int *pid) {
    int status = -1;
    int lfp = -1;
    char pidf[255] = DAEMON_PID_DIRECTORY;
    char str[10];
    DEBUG_FUNCTION;

    strcat(pidf, name);
    lfp = open(pidf, O_RDONLY);
    if (lfp < 0)
        goto out;
    if (read(lfp, str, 10) == -1)
        goto out;
    *pid = strtol(str, NULL, 10);

    status = 0;
out:
    return status;
}

static void daemon_handle_signal(int sig) {
    DEBUG_FUNCTION;

    switch (sig) {
    case SIGTERM:
        if (daemon_on_stop) {
            daemon_on_stop();
            signal(SIGTERM, SIG_DFL);
            raise(SIGTERM);
        }
        break;
    case SIGUSR1:
        if (daemon_on_usr1)
            daemon_on_usr1();
        break;
    }
}

void daemon_start(const char *name, void (*on_start) (void),
                  void (*on_stop) (void), void (*on_usr1) (void)) {
    int pid;
    DEBUG_FUNCTION;

    // check if running
    if (read_pid(name, &pid) == 0 && kill(pid, 0) == 0) {
        fprintf(stderr, "already running as pid: %d\n", pid);
        return;
    }
    if (daemon(0, 0) != 0) {
        perror("daemon failed");
        return;
    }
    if (on_stop)
        daemon_on_stop = on_stop;
    if (on_usr1)
        daemon_on_usr1 = on_usr1;
    if (write_pid(name) != 0) {
        return;
    }
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTERM, daemon_handle_signal);
    signal(SIGUSR1, daemon_handle_signal);
    on_start();
}

void daemon_stop(const char *name) {
    int pid;
    DEBUG_FUNCTION;

    if (read_pid(name, &pid) != 0) {
        fatal("read pid failed: %s", strerror(errno));
        return;
    }
    if (kill(pid, SIGTERM) != 0) {
        fatal("kill failed: %s", strerror(errno));
        return;
    }
}

void daemon_usr1(const char *name) {
    int pid;
    DEBUG_FUNCTION;

    if (read_pid(name, &pid) != 0) {
        fatal("read pid failed: %s", strerror(errno));
        return;
    }
    if (kill(pid, SIGUSR1) != 0) {
        fatal("kill failed: %s", strerror(errno));
        return;
    }
}
