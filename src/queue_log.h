/*
 * File:   queue_log.h
 * Sylvain David <sylvain.david@fizians.com>
 */
#ifndef QUEUE_LOG_H
#define	QUEUE_LOG_H

#include <stdlib.h>
#include <stdbool.h>

/*
 * Element of the queue
 */
typedef struct item {
    void *data;
    struct item *next;
} item_t;

/*
 * The queue
 */
typedef struct queue {
    size_t size;
    item_t *first;
    item_t *last;
    void (*destroy) (void *data);
} queue_t;

/*
 * Public interface
 */
void queue_init(queue_t * queue, void (*destroy) (void *data));
void queue_delete(queue_t * queue);
int queue_enqueue(queue_t * queue, const void *data);
void *queue_dequeue(queue_t * queue);
void *queue_peek(queue_t * queue);
bool queue_is_empty(queue_t * queue);
size_t queue_size(queue_t * queue);

#endif	/* QUEUE_LOG_H */

