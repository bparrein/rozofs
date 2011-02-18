#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pool.h"

// XXX COMPILATION TEST FOR NOW
int main(int argc, char** argv) {

	pool_t pool;
    pool_initialize(&pool);
    pool_release(&pool);

    return 0;
}
