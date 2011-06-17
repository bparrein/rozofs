/*
  Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
  This file is part of Rozo.

  Rozo is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  Rozo is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
 */

#ifndef _LOG_H
#define	_LOG_H

#include <syslog.h>
#include <libgen.h>

#define EDEBUG      0
#define EINFO       1
#define EWARNING    2
#define ESEVERE     3
#define EFATAL      4

static const char *messages[] =
    { "debug", "info", "warning", "severe", "fatal" };
static const int priorities[] =
    { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR, LOG_EMERG };

#define log(level, ...) \
    syslog(priorities[level], "%s - %d - %s", basename(__FILE__), __LINE__, messages[level]); \
    syslog(priorities[level], __VA_ARGS__)

#define info(...) log(EINFO, __VA_ARGS__)
#define warning(...) log(EWARNING, __VA_ARGS__)
#define severe(...) log(ESEVERE, __VA_ARGS__)
#define fatal(...) log(EFATAL, __VA_ARGS__)

#ifndef NDEBUG
#define DEBUG(...) log(EDEBUG, __VA_ARGS__)
#ifndef NDEBUGFUNCTION
#define DEBUG_FUNCTION DEBUG("%s", __FUNCTION__)
#else
#define DEBUG_FUNCTION
#endif
#else
#define DEBUG(...)
#define DEBUG_FUNCTION
#endif

#endif
