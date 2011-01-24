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

#ifndef _TRANSFORM_H
#define	_TRANSFORM_H

typedef struct angle {
    int p;	
    int q;
} angle_t;

typedef struct projection {
    angle_t angle;
    int size;
    char *bins;
} projection_t;

int transform_forward(const char *support, int rows, int cols, int np, projection_t *projections); 

int transform_inverse(char *support, int rows, int cols, int np, projection_t *projections);

#endif

