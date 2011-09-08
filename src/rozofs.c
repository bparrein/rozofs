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

#include <errno.h>

#include "xmalloc.h"
#include "log.h"
#include "rozofs.h"

uint8_t rozofs_safe;
uint8_t rozofs_forward;
uint8_t rozofs_inverse;
angle_t *rozofs_angles;
uint16_t *rozofs_psizes;

int rozofs_initialize(rozofs_layout_t layout) {
    int status = -1;
    int i;
    DEBUG_FUNCTION;

    switch (layout) {
    case LAYOUT_2_3_4:
        rozofs_safe = 4;
        rozofs_forward = 3;
        rozofs_inverse = 2;
        break;
    case LAYOUT_4_6_8:
        rozofs_safe = 8;
        rozofs_forward = 6;
        rozofs_inverse = 4;
        break;
    case LAYOUT_8_12_16:
        rozofs_safe = 16;
        rozofs_forward = 12;
        rozofs_inverse = 8;
        break;
    default:
        errno = EINVAL;
        goto out;
    }

    rozofs_angles = xmalloc(sizeof (angle_t) * rozofs_forward);
    rozofs_psizes = xmalloc(sizeof (uint16_t) * rozofs_forward);
    for (i = 0; i < rozofs_forward; i++) {
        rozofs_angles[i].p = i - rozofs_forward / 2;
        rozofs_angles[i].q = 1;
        rozofs_psizes[i] = abs(i - rozofs_forward / 2) * (rozofs_inverse - 1)
            + (ROZOFS_BSIZE / sizeof (pxl_t) / rozofs_inverse - 1) + 1;
    }
    status = 0;
out:
    return status;
}

void rozofs_release() {

    DEBUG_FUNCTION;

    free(rozofs_angles);
    free(rozofs_psizes);
}
