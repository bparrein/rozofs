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
 
#ifndef _LIST_H
#define _LIST_H

#include <stdlib.h>

typedef struct list {
    struct list *next, *prev;
} list_t;

static inline void list_init(list_t *list) {
    list->next = list; 
    list->prev = list;
}

static inline void list_insert(list_t *new, list_t *prev, list_t *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_push_front(list_t *head, list_t *new) {
    list_insert(new, head, head->next);
}

static inline void list_push_back(list_t *head, list_t *new) {
    list_insert(new, head->prev, head);
}

static inline void list_remove(list_t *list) {
    list->next->prev = list->prev;
    list->prev->next = list->next;
    list->next = (void *) 0;
    list->prev = (void *) 0;
}

static inline int list_empty(list_t *head) {
    return head->next == head;
}

static inline int list_size(list_t *head) {
    list_t *iterator;
    int size;
    for (iterator = (head)->next, size = 0; iterator != (head); iterator = iterator->next, size++);
    return size;
}

static inline void list_sort(list_t *head, int (*cmp)(list_t *a, list_t *b)) {
    list_t *p, *q, *e, *list, *tail, *oldhead;
    int insize, nmerges, psize, qsize, i;

    if (list_empty(head))
        return;

    list = head->next;
    list_remove(head);
    insize = 1;
    for (;;) {
        p = oldhead = list;
        list = tail = NULL;
        nmerges = 0;

        while (p) {
            nmerges++;
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
                q = q->next == oldhead ? NULL : q->next;
                if (!q)
                    break;
            }

            qsize = insize;
            while (psize > 0 || (qsize > 0 && q)) {
                if (!psize) {
                    e = q;
                    q = q->next;
                    qsize--;
                    if (q == oldhead) q = NULL;
                } else if (!qsize || !q) {
                    e = p;
                    p = p->next;
                    psize--;
                    if (p == oldhead) p = NULL;
                } else if (cmp(p, q) <= 0) {
                    e = p;
                    p = p->next;
                    psize--;
                    if (p == oldhead) p = NULL;
                } else {
                    e = q;
                    q = q->next;
                    qsize--;
                    if (q == oldhead) q = NULL;
                }
                if (tail)
                    tail->next = e;
                else
                    list = e;
                e->prev = tail;
                tail = e;
            }
            p = q;
        }

        tail->next = list;
        list->prev = tail;

        if (nmerges <= 1)
            break;

        insize *= 2;
    }

    head->next = list;
    head->prev = list->prev;
    list->prev->next = head;
    list->prev = head;
}

#define list_entry(ptr, type, member) ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each_forward(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_backward(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_forward_safe(pos, n, head)\
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_backward_safe(pos, n, head)\
    for (pos = (head)->prev, n = pos->prev; pos != (head); pos = n, n = pos->prev)

#endif
