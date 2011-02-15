#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "distribution.h"

int main(int argc, char** argv) {

    int i;
    distribution_t d = 0;

    for (i = 0; i < 16; i++) {
        printf("%x ", distribution_is_set(d, i));
    }
    printf("\n");

    distribution_set_true(d, 4);
    distribution_set_true(d, 7);
    distribution_set_true(d, 9);

    for (i = 0; i < 16; i++) {
        printf("%x ", distribution_is_set(d, i));
    }
    printf("\n");

    distribution_set_false(d, 4);
    distribution_set_false(d, 9);
    distribution_set_true(d, 3);
    distribution_set_true(d, 5);
    distribution_set_true(d, 7);
    distribution_set_true(d, 8);
    for (i = 0; i < 16; i++) {
        printf("%x ", distribution_is_set(d, i));
    }
    printf("\n");
    for (i = 0; i < 16; i++) {
        printf("%x ", distribution_is_set(d, i));
    }
    printf("\n");

    distribution_set_value(d, 3, 0);
    distribution_set_value(d, 4, 1);
    distribution_set_value(d, 5, 0);
    distribution_set_value(d, 8, 0);
    distribution_set_value(d, 9, 1);
    for (i = 0; i < 16; i++) {
        printf("%x ", distribution_is_set(d, i));
    }
    printf("\n");

    return 0;
}
