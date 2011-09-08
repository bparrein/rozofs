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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "htable.h"

static unsigned int string_hash(void *key) {
    int hash = 0;
    char *c;

    for (c = key; *c != '\0'; c++)
        hash = *c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

static int string_cmp(void *key1, void *key2) {
    return strcmp((char *) key1, (char *) key2);
}

int main(int argc, char **argv) {

    int i;
    htable_t h;
    void *ptr;

    static char *keys[] = { "a", "b", "c", "d", "e",
        "f", "g", "h", "i", "j", "k", "l",
        "m", "n", "o", "p", "q", "r", "s",
        "t", "u", "v", "w", "x", "y", "z"
    };

    static char *vals[] = { "alpha", "bravo", "charlie", "delta", "echo",
        "foxtrot", "golf", "hotel", "india", "juliet", "kilo", "lima",
        "mike", "november", "oscar", "papa", "quebec", "romeo", "sierra",
        "tango", "uniform", "victor", "whisky", "x-ray", "yankee", "zulu"
    };

    htable_initialize(&h, 10, string_hash, string_cmp);

    ptr = htable_get(&h, "z");

    for (i = 0; i < 26; i++)
        htable_put(&h, keys[i], vals[i]);

    for (i = 0; i < 26; i++)
        printf("%s: %s\n", keys[i], (char *) htable_get(&h, keys[i]));

    printf("hello world: %s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n",
           (char *) htable_get(&h, "h"), (char *) htable_get(&h, "e"),
           (char *) htable_get(&h, "l"), (char *) htable_get(&h, "l"),
           (char *) htable_get(&h, "o"), (char *) htable_get(&h, "w"),
           (char *) htable_get(&h, "o"), (char *) htable_get(&h, "r"),
           (char *) htable_get(&h, "l"), (char *) htable_get(&h, "d"));

    htable_del(&h, "z");
    for (i = 0; i < 25; i++)
        printf("%s: %s\n", keys[i], (char *) htable_get(&h, keys[i]));

    htable_get(&h, "z");

    htable_release(&h);

    return 0;
}
