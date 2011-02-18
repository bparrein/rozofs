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

#include <netdb.h>
#include <netinet/tcp.h>
#include <errno.h>

#include "log.h"
#include "rpc_client.h"

int rpc_client_initialize(rpc_client_t *client, const char *host, unsigned long prog, unsigned long vers,
        unsigned int sendsz, unsigned int recvsz) {

    int status;
    struct sockaddr_in server;
    struct hostent *hp;
    int one = 1;
    int port;

    DEBUG_FUNCTION;

    client->client = NULL;
    client->sock = -1;

    server.sin_family = AF_INET;

    if ((hp = gethostbyname(host)) == 0) {
        status = -1;
        severe("gethostbyname failed for host : %s, %s", host, strerror(errno));
        goto out;
    }
    bcopy((char *) hp->h_addr, (char *) & server.sin_addr, hp->h_length);

    if ((port = pmap_getport(&server, prog, vers, IPPROTO_TCP)) == 0) {
        status = -1;
        errno = EAGAIN; /* Try again */
        warning("pmap_getport failed%s", clnt_spcreateerror(""));
        goto out;
    }

    server.sin_port = htons(port);

    if ((client->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        status = -1;
        goto out;
    }

    if (setsockopt(client->sock, SOL_SOCKET, SO_REUSEADDR, (void *) & one, sizeof (one)) < 0) {
        status = -1;
        goto out;
    }

    struct linger linger;
    linger.l_onoff = 1; //0 = off (l_linger ignored), nonzero = on
    linger.l_linger = 0; //0 = discard data, nonzero = wait for data sent

    if (setsockopt(client->sock, SOL_SOCKET, SO_LINGER, &linger, sizeof (linger)) < 0) {
        status = -1;
        goto out;
    }

    if (setsockopt(client->sock, SOL_TCP, TCP_NODELAY, (void *) & one, sizeof (one)) < 0) {
        status = -1;
        goto out;
    }

    if ((client->sock < 0) || (connect(client->sock, (struct sockaddr *) & server, sizeof (server)) < 0)) {
        status = -1;
        goto out;
    }

    if ((client->client = clnttcp_create(&server, prog, vers, &client->sock, sendsz, recvsz)) == NULL) {
        status = -1;
        goto out;
    }

    // Set TIMEOUT for this connection
    // timeout_set is the waiting time for detect a problem with the remote server
    struct timeval timeout_set;
    timeout_set.tv_sec = 3; // change time-out to 3s
    timeout_set.tv_usec = 0;
    clnt_control(client->client, CLSET_TIMEOUT, (char *) &timeout_set);

    status = 0;
out:
    if (status != 0) rpc_client_release(client);
    return status;
}

void rpc_client_release(rpc_client_t *client) {

    DEBUG_FUNCTION;

    if (client->sock <= 0) goto out;

    if ((close(client->sock)) != 0) goto out;
    client->sock = -1;

    if (! client->client) goto out;
    clnt_destroy(client->client);
    client->client = NULL;

out:
    return;
}
