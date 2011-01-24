#include <stdlib.h>
#include <stdio.h>

#include "storage.h"

// XXX COMPILATION TEST FOR NOW
int main(int argc, char** argv) {

    if (!rozo_initialize()) {
        perror("failed to initialize rozo");
        exit(-1);
    }
    exit(0);
}
