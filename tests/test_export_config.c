#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "export_config.h"

int main(int argc, char** argv) {

    export_config_t config;

    if (argc < 2) {
	printf("test_config <file>\n");
	return -1;
    }

    if (export_config_initialize(&config, argv[1]) != 0) {
	perror("failed to initialize config");
	return -1;
    }

    export_config_print(&config);

    export_config_release(&config);

    return 0;
}
