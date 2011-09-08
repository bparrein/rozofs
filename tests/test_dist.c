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

#include <stdio.h>
#include "dist.h"

int main(int argc, char **argv) {

    int i;
    dist_t d = 0;

    for (i = 0; i < 16; i++) {
        printf("%x ", dist_is_set(d, i));
    }
    printf("\n");

    dist_set_true(d, 4);
    dist_set_true(d, 7);
    dist_set_true(d, 9);

    for (i = 0; i < 16; i++) {
        printf("%x ", dist_is_set(d, i));
    }
    printf("\n");

    dist_set_false(d, 4);
    dist_set_false(d, 9);
    dist_set_true(d, 3);
    dist_set_true(d, 5);
    dist_set_true(d, 7);
    dist_set_true(d, 8);
    for (i = 0; i < 16; i++) {
        printf("%x ", dist_is_set(d, i));
    }
    printf("\n");
    for (i = 0; i < 16; i++) {
        printf("%x ", dist_is_set(d, i));
    }
    printf("\n");

    dist_set_value(d, 3, 0);
    dist_set_value(d, 4, 1);
    dist_set_value(d, 5, 0);
    dist_set_value(d, 8, 0);
    dist_set_value(d, 9, 1);
    for (i = 0; i < 16; i++) {
        printf("%x ", dist_is_set(d, i));
    }
    printf("\n");

    return 0;
}
