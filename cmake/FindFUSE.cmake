# Copyright (c) 2010 Fizians SAS. <http://www.fizians.com>
# This file is part of Rozofs.
#
# Rozofs is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 3 of the License,
# or (at your option) any later version.
#
# Rozofs is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

# - Find fuse
# Find the native FUSE includes and library
#
#  FUSE_INCLUDE_DIR - where to find fuse.h, etc.
#  FUSE_LIBRARIES   - List of libraries when using fuse.
#  FUSE_FOUND       - True if fuse found.

FIND_PATH(FUSE_INCLUDE_DIR fuse.h
  /usr/local/include/fuse
  /usr/local/include
  /usr/include/fuse
  /usr/include
)

SET(FUSE_NAMES fuse)
FIND_LIBRARY(FUSE_LIBRARY
  NAMES ${FUSE_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF(FUSE_INCLUDE_DIR AND FUSE_LIBRARY)
  SET(FUSE_FOUND TRUE)
  SET(FUSE_LIBRARIES ${FUSE_LIBRARY} )
ELSE(FUSE_INCLUDE_DIR AND FUSE_LIBRARY)
  SET(FUSE_FOUND FALSE)
  SET(FUSE_LIBRARIES)
ENDIF(FUSE_INCLUDE_DIR AND FUSE_LIBRARY)

IF(NOT FUSE_FOUND)
   IF(FUSE_FIND_REQUIRED)
     MESSAGE(FATAL_ERROR "fuse library and headers required.")
   ENDIF(FUSE_FIND_REQUIRED)
ENDIF(NOT FUSE_FOUND)

MARK_AS_ADVANCED(
  FUSE_LIBRARY
  FUSE_INCLUDE_DIR
)
