# Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
# This file is part of Rozo.
#
# Rozo is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 3 of the License,
# or (at your option) any later version.
#
# Rozo is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

# - Find pthread
# Find the native PTHREAD includes and library
#
#  PTHREAD_INCLUDE_DIR - where to find pthread.h, etc.
#  PTHREAD_LIBRARIES   - List of libraries when using pthread.
#  PTHREAD_FOUND       - True if pthread found.

FIND_PATH(PTHREAD_INCLUDE_DIR pthread.h
  /usr/local/include/attr
  /usr/local/include
  /usr/include/attr
  /usr/include
)

SET(PTHREAD_NAMES pthread)
FIND_LIBRARY(PTHREAD_LIBRARY
  NAMES ${PTHREAD_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF(PTHREAD_INCLUDE_DIR AND PTHREAD_LIBRARY)
  SET(PTHREAD_FOUND TRUE)
  SET(PTHREAD_LIBRARIES ${PTHREAD_LIBRARY} )
ELSE(PTHREAD_INCLUDE_DIR AND PTHREAD_LIBRARY)
  SET(PTHREAD_FOUND FALSE)
  SET(PTHREAD_LIBRARIES)
ENDIF(PTHREAD_INCLUDE_DIR AND PTHREAD_LIBRARY)

IF(NOT PTHREAD_FOUND)
   IF(PTHREAD_FIND_REQUIRED)
     MESSAGE(FATAL_ERROR "pthread library and headers required.")
   ENDIF(PTHREAD_FIND_REQUIRED)
ENDIF(NOT PTHREAD_FOUND)

MARK_AS_ADVANCED(
  PTHREAD_LIBRARY
  PTHREAD_INCLUDE_DIR
)
