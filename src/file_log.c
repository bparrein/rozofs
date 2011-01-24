#include <stdio.h>

#include "log.h"
#include "file_log.h"

static char *log = ".logfile_mfs";

int log_remove_projection(item_proj_t * proj) {

    int status;
    FILE * file_log;
    char uuid_file[37];
    char uuid_ms[37];

    DEBUG_FUNCTION;

    if ((file_log = fopen(log, "a")) == NULL) {
        status = -1;
        goto out;
    }

    uuid_file[36] = '\0';
    uuid_ms[36] = '\0';
    uuid_unparse(proj->mf, uuid_file);
    uuid_unparse(proj->uuid_ms, uuid_ms);

    status = 0;
out:
    if (file_log != NULL) {
        fclose(file_log);
    }
    return status;
}

int log_remove_file(item_file_t * file) {

    int status;
    FILE * file_log;
    char uuid_file[37];
    char uuid_ms[37];

    DEBUG_FUNCTION;

    if ((file_log = fopen(log, "a")) == NULL) {
        status = -1;
        goto out;
    }

    uuid_file[36] = '\0';
    uuid_ms[36] = '\0';
    uuid_unparse(file->mf, uuid_file);
    uuid_unparse(file->uuid_ms, uuid_ms);

    status = 0;
    
out:
    if (file_log != NULL) {
        fclose(file_log);
    }
    return status;
}
