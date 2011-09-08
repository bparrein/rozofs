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

# - Find uuid
# Find the native UUID includes and library
#
#  UUID_INCLUDE_DIR - where to find uuid.h, etc.
#  UUID_LIBRARIES   - List of libraries when using uuid.
#  UUID_FOUND       - True if uuid found.

FIND_PATH(UUID_INCLUDE_DIR uuid.h
  /usr/local/include/uuid
  /usr/local/include
  /usr/include/uuid
  /usr/include
)

SET(UUID_NAMES uuid)
FIND_LIBRARY(UUID_LIBRARY
  NAMES ${UUID_NAMES}
  PATHS /usr/lib /usr/local/lib
)

IF(UUID_INCLUDE_DIR AND UUID_LIBRARY)
  SET(UUID_FOUND TRUE)
  SET(UUID_LIBRARIES ${UUID_LIBRARY} )
ELSE(UUID_INCLUDE_DIR AND UUID_LIBRARY)
  SET(UUID_FOUND FALSE)
  SET(UUID_LIBRARIES)
ENDIF(UUID_INCLUDE_DIR AND UUID_LIBRARY)

IF(NOT UUID_FOUND)
   IF(UUID_FIND_REQUIRED)
     MESSAGE(FATAL_ERROR "uuid library and headers required.")
   ENDIF(UUID_FIND_REQUIRED)
ENDIF(NOT UUID_FOUND)

MARK_AS_ADVANCED(
  UUID_LIBRARY
  UUID_INCLUDE_DIR
)
