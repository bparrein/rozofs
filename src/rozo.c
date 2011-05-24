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

#include <errno.h>

#include "xmalloc.h"
#include "log.h"
#include "rozo.h"

uint8_t rozo_safe;
uint8_t rozo_forward;
uint8_t rozo_inverse;
angle_t *rozo_angles;
uint16_t *rozo_psizes;

int rozo_initialize(rozo_layout_t layout) {
    int status = -1;
    int i;
    DEBUG_FUNCTION;

    switch (layout) {
    case LAYOUT_2_3_4:
        rozo_safe = 4;
        rozo_forward = 3;
        rozo_inverse = 2;
        break;
    case LAYOUT_4_6_8:
        rozo_safe = 8;
        rozo_forward = 6;
        rozo_inverse = 4;
        break;
    case LAYOUT_8_12_16:
        rozo_safe = 16;
        rozo_forward = 12;
        rozo_inverse = 8;
        break;
    default:
        errno = EINVAL;
        goto out;
    }

    rozo_angles = xmalloc(sizeof (angle_t) * rozo_forward);
    rozo_psizes = xmalloc(sizeof (uint16_t) * rozo_forward);
    for (i = 0; i < rozo_forward; i++) {
        rozo_angles[i].p = i - rozo_forward / 2;
        rozo_angles[i].q = 1;
        rozo_psizes[i] = abs(i - rozo_forward / 2) * (rozo_inverse - 1)
            + (ROZO_BSIZE / sizeof (pxl_t) / rozo_inverse - 1) + 1;
    }
    status = 0;
out:
    return status;
}

void rozo_release() {

    DEBUG_FUNCTION;

    free(rozo_angles);
    free(rozo_psizes);
}
