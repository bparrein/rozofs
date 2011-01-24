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

#ifndef _ROZO_H
#define _ROZO_H

#include <stdint.h>

#include "transform.h"

// number of host needed to store
#define ROZO_SAFE 16

// total number of projections
#define ROZO_FORWARD 12

// number of projections needed to reconstruct
#define ROZO_INVERSE 8

// transform block size
#define ROZO_BSIZE 8192

#define ROZO_HOSTNAME_MAX 128

#define ROZO_UUID_SIZE 16

#define ROZO_PATH_MAX 1024

#define ROZO_FILENAME_MAX 255

extern angle_t rozo_angles[ROZO_FORWARD];

extern int16_t rozo_psizes[ROZO_FORWARD];

extern uint8_t empty_distribution[ROZO_SAFE];

int rozo_initialize();

#endif
