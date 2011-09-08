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

#include <string.h>

#include "xmalloc.h"
#include "log.h"
#include "list.h"
#include "htable.h"

typedef struct hash_entry {
    void *key;
    void *value;
    list_t list;
} hash_entry_t;

void htable_initialize(htable_t * h, uint32_t size, uint32_t(*hash) (void *),
                       int (*cmp) (void *, void *)) {
    list_t *it;
    DEBUG_FUNCTION;

    h->hash = hash;
    h->cmp = cmp;
    h->size = size;
    h->buckets = xmalloc(size * sizeof (list_t));
    for (it = h->buckets; it != h->buckets + size; it++)
        list_init(it);
}

void htable_release(htable_t * h) {
    list_t *it;
    DEBUG_FUNCTION;

    for (it = h->buckets; it != h->buckets + h->size; it++) {
        list_t *p, *q;
        list_for_each_forward_safe(p, q, it) {
            hash_entry_t *he = list_entry(p, hash_entry_t, list);
            list_remove(p);
            free(he);
        }
    }
    free(h->buckets);
    h->buckets = 0;
    h->hash = 0;
    h->cmp = 0;
    h->size = 0;

    return;
}

void htable_put(htable_t * h, void *key, void *value) {
    list_t *bucket, *p;
    hash_entry_t *he = 0;

    DEBUG_FUNCTION;

    bucket = h->buckets + (h->hash(key) % h->size);
    // If entry exits replace value.
    list_for_each_forward(p, bucket) {
        he = list_entry(p, hash_entry_t, list);
        if (h->cmp(he->key, key) == 0) {
            he->value = value;
            return;
        }
    }

    // Else create a new one.
    he = xmalloc(sizeof (hash_entry_t));
    he->key = key;
    he->value = value;
    list_init(&he->list);
    list_push_back(bucket, &he->list);
}

void *htable_get(htable_t * h, void *key) {
    list_t *p;
    DEBUG_FUNCTION;

    list_for_each_forward(p, h->buckets + (h->hash(key) % h->size)) {
        hash_entry_t *he = list_entry(p, hash_entry_t, list);
        if (h->cmp(he->key, key) == 0) {
            return he->value;
        }
    }
    return 0;
}

// Return the removed value or NULL if not found.
void *htable_del(htable_t * h, void *key) {
    void *value = NULL;
    list_t *p, *q;
    DEBUG_FUNCTION;

    list_for_each_forward_safe(p, q, h->buckets + (h->hash(key) % h->size)) {
        hash_entry_t *he = list_entry(p, hash_entry_t, list);
        if (h->cmp(he->key, key) == 0) {
            value = he->value;
            list_remove(p);
            free(he);
            break;
        }
    }

    return value;
}
