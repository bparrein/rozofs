/* 
 * File:   file_log.h
 * Sylvain David <sylvain.david@fizians.com>
 */

#ifndef FILE_LOG_H
#define	FILE_LOG_H

#include <stdint.h>
#include <limits.h>
#include <uuid/uuid.h>

/*
 * Description for a projection
 */
typedef struct item_proj {
    char file_path[PATH_MAX];
    uuid_t uuid_ms;
    uuid_t mf;
    uint8_t mp;
    uint64_t mb;
} item_proj_t;

/*
 * Description for a file
 */
typedef struct item_file {
    char file_path[PATH_MAX];
    uuid_t uuid_ms;
    uuid_t mf;
} item_file_t;

/*
 * Public interface
 */
int log_remove_projection(item_proj_t * proj);
int log_remove_file(item_file_t * file);

#endif	/* FILE_LOG_H */
