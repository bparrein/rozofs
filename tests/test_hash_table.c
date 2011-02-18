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
#include <stdio.h>
#include <string.h>

#include "hash_table.h"

static int string_hash(void *key) {
	return hash_table_hash(key, strlen(key));
}

static int string_cmp(void *key1, void *key2) {
	return strcmp((char *)key1, (char *)key2);
}

int main(int argc, char** argv) {

    int i;
    hash_table_t h;

    static char *keys[] = { "a", "b", "c", "d", "e",
    			"f", "g", "h", "i", "j", "k", "l",
    			"m", "n", "o", "p", "q", "r", "s",
    			"t", "u", "v", "w", "x", "y", "z" };

    static char *vals[] = { "alpha", "bravo", "charlie", "delta", "echo",
			"foxtrot", "golf", "hotel", "india", "juliet", "kilo", "lima",
			"mike", "november", "oscar", "papa", "quebec", "romeo", "sierra",
			"tango", "uniform", "victor", "whisky", "x-ray", "yankee", "zulu" };

    hash_table_init(&h, 10, string_hash, string_cmp);

    for (i = 0; i < 26; i++)
    	hash_table_put(&h, keys[i], vals[i]);

    for (i = 0; i < 26; i++)
    	printf("%s: %s\n", keys[i], (char *)hash_table_get(&h, keys[i]));

    printf("hello world: %s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n",
    		(char *)hash_table_get(&h, "h"),
    		(char *)hash_table_get(&h, "e"),
    		(char *)hash_table_get(&h, "l"),
    		(char *)hash_table_get(&h, "l"),
    		(char *)hash_table_get(&h, "o"),
    		(char *)hash_table_get(&h, "w"),
    		(char *)hash_table_get(&h, "o"),
    		(char *)hash_table_get(&h, "r"),
    		(char *)hash_table_get(&h, "l"),
    		(char *)hash_table_get(&h, "d"));

    hash_table_del(&h, "z");
    for (i = 0; i < 25; i++)
        	printf("%s: %s\n", keys[i], (char *)hash_table_get(&h, keys[i]));

}
