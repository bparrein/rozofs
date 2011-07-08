/*
 * profile.h
 *
 *  Created on: 21 mars 2011
 *      Author: Boris Lucas
 */

#ifndef PROFILE_H_
#define PROFILE_H_

#include <time.h>

#define RESULT_FILE "/home/fizians/rozo_profile.out"

typedef struct timeval timeval;
timeval *profile_time;
timeval *t_profile_start;
timeval *t_profile_stop;
timeval *t_profile_res;

#ifdef PROFILE

#define PROFILE_IMPORT {extern timeval *profile_time;}

#define PROFILE_INIT {profile_time = (timeval*) malloc(5* sizeof(timeval)); \
						profile_time[0].tv_sec = 0;  \
						profile_time[0].tv_usec = 0; \
						profile_time[1].tv_sec = 0;  \
						profile_time[1].tv_usec = 0; \
						profile_time[2].tv_sec = 0;  \
						profile_time[2].tv_usec = 0; \
						profile_time[3].tv_sec = 0;  \
						profile_time[3].tv_usec = 0; \
						t_profile_start = (timeval*) malloc(sizeof(timeval) +1); \
						t_profile_stop = (timeval*) malloc(sizeof(timeval) +1); \
						t_profile_res = (timeval*) malloc(sizeof(timeval) +1); }

#define PROFILE_DECLARE {extern timeval * t_profile_start; \
						 extern timeval * t_profile_stop; \
						extern timeval * t_profile_res;}

#define PROFILE_EXPORT_START gettimeofday(t_profile_start, NULL);

#define PROFILE_EXPORT_STOP gettimeofday(t_profile_stop, NULL); \
							timeval_subtract(t_profile_res, t_profile_stop, t_profile_start ); \
							timeval_addto(&profile_time[1], t_profile_res);

#define PROFILE_STORAGE_START gettimeofday(t_profile_start, NULL);

#define PROFILE_STORAGE_STOP gettimeofday(t_profile_stop, NULL); \
							timeval_subtract(t_profile_res, t_profile_stop, t_profile_start ); \
							timeval_addto(&profile_time[0], t_profile_res);

#define PROFILE_TRANSFORM_START gettimeofday(t_profile_start, NULL);

#define PROFILE_TRANSFORM_FRWD_STOP gettimeofday(t_profile_stop, NULL); \
							timeval_subtract(t_profile_res, t_profile_stop, t_profile_start ); \
							timeval_addto(&profile_time[2], t_profile_res);

#define PROFILE_TRANSFORM_INV_STOP gettimeofday(t_profile_stop, NULL); \
							timeval_subtract(t_profile_res, t_profile_stop, t_profile_start ); \
							timeval_addto(&profile_time[3], t_profile_res);

#define PROFILE_PRINT log_profile()

#define PROFILE_STORAGE_PRINT log_storage_profile()

#define PROFILE_SAVE save_profile_result(RESULT_FILE)

#define PROFILE_DELETE free(profile_time)

#else

#define PROFILE_IMPORT

#define PROFILE_INIT

#define PROFILE_DECLARE

#define PROFILE_EXPORT_START

#define PROFILE_EXPORT_STOP

#define PROFILE_STORAGE_START

#define PROFILE_STORAGE_STOP

#define PROFILE_TRANSFORM_START

#define PROFILE_TRANSFORM_FRWD_STOP

#define PROFILE_TRANSFORM_INV_STOP

#define PROFILE_PRINT

#define PROFILE_STORAGE_PRINT

#define PROFILE_SAVE

#define PROFILE_DELETE

#endif

#endif                          /* PROFILE_H_ */
