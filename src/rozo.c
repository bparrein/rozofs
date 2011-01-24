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
#include <string.h>
#include <errno.h>

#include "log.h"
#include "rozo.h"

angle_t rozo_angles[ROZO_FORWARD];

int16_t rozo_psizes[ROZO_FORWARD];

uint8_t empty_distribution[ROZO_SAFE];

int rozo_initialize() {

    int status, i;

    DEBUG_FUNCTION;

    if (ROZO_BSIZE % ROZO_INVERSE != 0) {
        errno = EINVAL;
        status = -1;
        goto out;
    }

    // transform inverse optim
    if ((ROZO_BSIZE / ROZO_INVERSE) % 8 != 0) {
        errno = EINVAL;
        status = -1;
        goto out;
    }

    for (i = 0; i < ROZO_FORWARD; i++) {
        rozo_angles[i].p = i - ROZO_FORWARD / 2;
        rozo_angles[i].q = 1;
        rozo_psizes[i] = abs(i - ROZO_FORWARD / 2) *
                (ROZO_INVERSE - 1) + (ROZO_BSIZE / ROZO_INVERSE - 1) + 1;
    }

    // Distribution filled with 0
    memset(empty_distribution, 0, ROZO_SAFE);

    status = 0;

out:
    return status;
}
