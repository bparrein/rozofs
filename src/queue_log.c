#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "queue_log.h"

void queue_init(queue_t * queue, void (*destroy) (void *data)) {
    queue->size = 0;
    queue->first = NULL;
    queue->last = NULL;
    queue->destroy = destroy;
}

void queue_delete(queue_t * queue) {

    // While the queue is not empty
    while (queue->size > 0) {

        item_t *old_first_item = queue->first;

        // Remove the first element of the queue
        queue->first = old_first_item->next;
        queue->size--;

        // If it's necessary to have a function to destroy the 
        // data part of a element
        if (queue->destroy != NULL) {
            queue->destroy(old_first_item->data);
        }
        // Destroy the element
        free(old_first_item);
        old_first_item = NULL;
    }

    memset(queue, 0, sizeof (queue));

    return;
}

int queue_enqueue(queue_t * queue, const void *data) {

    // Creation and initialization of the new element
    item_t *new_item = NULL;

    if ((new_item = malloc(sizeof (item_t))) == NULL) {
        return -1;
    }

    memset(new_item, 0, sizeof (item_t));

    // Assignment of data part of the new element
    new_item->data = (void *) data;

    // Insertion in the queue: it will be his last new element
    // The list may be empty !
    if (queue->last != NULL) {
        queue->last->next = new_item;
    }
    if (queue->first == NULL) {
        queue->first = new_item;
    }
    queue->last = new_item;
    queue->size++;

    return 0;
}

void * queue_dequeue(queue_t * queue) {

    // If the queue is empty it returns NULL
    if (queue->size == 0) {
        return NULL;
    }

    // We dequeue the first element
    item_t *dequeued_item = queue->first;
    queue->first = dequeued_item->next;
    queue->size--;

    // We retrieve the data of this element and then delete it
    void *data = dequeued_item->data;
    free(dequeued_item);
    dequeued_item = NULL;

    return data;
}

void * queue_peek(queue_t * queue) {

    // If the queue is empty it returns NULL
    if (queue->size == 0) {
        return NULL;
    }
    return queue->first->data;
}

bool queue_is_empty(queue_t * queue) {

    if (queue->size == 0) {
        return true;
    } else {
        return false;
    }
}

size_t queue_size(queue_t * queue) {
    return queue->size;
}
