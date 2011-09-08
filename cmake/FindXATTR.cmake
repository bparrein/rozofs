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

# - Find xattr
# Find the native XATTR includes and library
#
#  XATTR_INCLUDE_DIR - where to find xattr.h, etc.
#  XATTR_LIBRARIES   - List of libraries when using xattr.
#  XATTR_FOUND       - True if xattr found.

FIND_PATH(XATTR_INCLUDE_DIR xattr.h
  /usr/local/include/attr
  /usr/local/include
  /usr/include/attr
  /usr/include
)

SET(XATTR_NAMES attr)
FIND_LIBRARY(XATTR_LIBRARY
  NAMES ${XATTR_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF(XATTR_INCLUDE_DIR AND XATTR_LIBRARY)
  SET(XATTR_FOUND TRUE)
  SET(XATTR_LIBRARIES ${XATTR_LIBRARY} )
ELSE(XATTR_INCLUDE_DIR AND XATTR_LIBRARY)
  SET(XATTR_FOUND FALSE)
  SET(XATTR_LIBRARIES)
ENDIF(XATTR_INCLUDE_DIR AND XATTR_LIBRARY)

IF(NOT XATTR_FOUND)
   IF(XATTR_FIND_REQUIRED)
     MESSAGE(FATAL_ERROR "xattr library and headers required.")
   ENDIF(XATTR_FIND_REQUIRED)
ENDIF(NOT XATTR_FOUND)

MARK_AS_ADVANCED(
  XATTR_LIBRARY
  XATTR_INCLUDE_DIR
)
