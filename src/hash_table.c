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

#include <string.h>
#include "list.h"
#include "hash_table.h"

typedef struct hash_entry {
	void *key;
	void *value;
	list_t list;
} hash_entry_t;

int hash_table_hash(void *key, int len) {
	int hash = 0;
    char *c;

    for (c = key; c != key + len; c++)
    	hash = *c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

int hash_table_init(hash_table_t *h, int size, int (*hash) (void *), int (*cmp) (void *, void *)) {

	int status = -1;
	list_t *it;

	h->hash = hash;
	h->cmp = cmp;
	h->size = size;
	if (!(h->buckets = malloc(size * sizeof(list_t)))) {
		goto out;
	}

	for (it = h->buckets; it != h->buckets + size; it++) {
		list_init(it);
	}

	status = 0;
out:
	return status;
}

void hash_table_release(hash_table_t *h) {

	list_t *it;

	if (! h) goto out;

	for (it = h->buckets; it != h->buckets + h->size; it++) {
		list_t *p, *q;
		list_for_each_forward_safe(p, q, it) {
			hash_entry_t *he = list_entry(p, hash_entry_t, list);
			list_remove(p);
			free(he);
		}
	}

out:
	return;
}

int hash_table_put(hash_table_t *h, void *key, void *value) {

	int status;
	hash_entry_t *he = NULL;

	if (!(he = malloc(sizeof(hash_entry_t)))) {
		goto out;
	}

	he->key = key;
	he->value = value;
	list_init(&he->list);
	list_push_back(h->buckets + (h->hash(key) % h->size), &he->list);

	status = 0;
out:
	return status;
}

void * hash_table_get(hash_table_t *h, void *key) {

	void *value = NULL;
	list_t *p;

	list_for_each_forward(p, h->buckets + (h->hash(key) % h->size)) {
		hash_entry_t *he = list_entry(p, hash_entry_t, list);
		if (h->cmp(he->key, key) == 0) {
			value = he->value;
			break;
		}
	}

out:
	return value;
}

// Return the removed value or NULL if not found.
void * hash_table_del(hash_table_t *h, void *key) {

	void *value = NULL;
	list_t *p, *q;

	list_for_each_forward_safe(p, q, h->buckets + (h->hash(key) % h->size)) {
		hash_entry_t *he = list_entry(p, hash_entry_t, list);
		if (h->cmp(he->key, key) == 0) {
			value = he->value;
			list_remove(p);
			free(he);
			break;
		}
	}

out:
	return value;
}
