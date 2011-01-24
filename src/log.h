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

#define log(priority, ...) \
    syslog(priority, "%s - %d", basename(__FILE__), __LINE__); \
    syslog(priority, __VA_ARGS__)

#define info(...) log(LOG_INFO, __VA_ARGS__)
#define warning(...) log(LOG_WARNING, __VA_ARGS__)
#define severe(...) log(LOG_ERR, __VA_ARGS__)
#define fatal(...) log(LOG_EMERG, __VA_ARGS__)

#ifndef NDEBUG
#define DEBUG(...) log(LOG_DEBUG, __VA_ARGS__)
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
