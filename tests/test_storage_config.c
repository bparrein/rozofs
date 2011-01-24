#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "storage_config.h"

int main(int argc, char** argv) {

    storage_config_t config;

    if (argc < 2) {
	printf("test_storage_config <file>\n");
	return -1;
    }

    if (storage_config_initialize(&config, argv[1]) != 0) {
	perror("failed to initialize config");
	return -1;
    }

    storage_config_print(&config);

    storage_config_release(&config);

    return 0;
}
