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

# - Find config
# Find the native CONFIG includes and library
#
#  CONFIG_INCLUDE_DIR - where to find config.h, etc.
#  CONFIG_LIBRARIES   - List of libraries when using config.
#  CONFIG_FOUND       - True if config found.

FIND_PATH(CONFIG_INCLUDE_DIR libconfig.h
  /usr/local/include
  /usr/include
)

SET(CONFIG_NAMES config)
FIND_LIBRARY(CONFIG_LIBRARY
  NAMES ${CONFIG_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF(CONFIG_INCLUDE_DIR AND CONFIG_LIBRARY)
  SET(CONFIG_FOUND TRUE)
  SET(CONFIG_LIBRARIES ${CONFIG_LIBRARY} )
ELSE(CONFIG_INCLUDE_DIR AND CONFIG_LIBRARY)
  SET(CONFIG_FOUND FALSE)
  SET(CONFIG_LIBRARIES)
ENDIF(CONFIG_INCLUDE_DIR AND CONFIG_LIBRARY)

IF(NOT CONFIG_FOUND)
   IF(CONFIG_FIND_REQUIRED)
     MESSAGE(FATAL_ERROR "libconfig library and headers required.")
   ENDIF(CONFIG_FIND_REQUIRED)
ENDIF(NOT CONFIG_FOUND)

MARK_AS_ADVANCED(
  CONFIG_LIBRARY
  CONFIG_INCLUDE_DIR
)
