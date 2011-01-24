#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "list.h"

typedef struct entry {
    list_t list;
    int value;
} entry_t;

static int cmp(list_t *l1, list_t *l2) {
    entry_t *e1 = list_entry(l1, entry_t, list);
    entry_t *e2 = list_entry(l2, entry_t, list);

    return e1->value < e2->value;
}

static void print_list(list_t *list) {

    list_t *iterator;

    printf("============================================\n");
    list_for_each_forward(iterator, list) {
        entry_t *entry = list_entry(iterator, entry_t, list);
        printf("%d\n", entry->value);
    }
};

int main(int argc, char** argv) {

    list_t head;
    entry_t e1;
    entry_t e2;
    entry_t e3;
    entry_t e4;

    list_init(&head);

    print_list(&head);

    list_init(&e1.list);
    e1.value = 5;
    list_push_back(&head, &e1.list);
    print_list(&head);

    list_init(&e2.list);
    e2.value = 3;
    list_push_front(&head, &e2.list);
    print_list(&head);

    list_init(&e3.list);
    e3.value = 8;
    list_push_front(&head, &e3.list);
    print_list(&head);

    list_init(&e4.list);
    e4.value = 6;
    list_push_back(&head, &e4.list);
    print_list(&head);

    list_sort(&head, cmp);
    print_list(&head);

    return 0;
}
