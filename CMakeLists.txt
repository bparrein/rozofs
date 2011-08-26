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

#
# Project settings.
#
cmake_minimum_required(VERSION 2.6)
project(rozo C)
set(MAJOR "0")
set(MINOR "3")
set(REVISION "7")
set(VERSION "${MAJOR}.${MINOR}.${REVISION}")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")

#
# Find packages
#
find_package(UUID REQUIRED)
find_package(CONFIG REQUIRED)
find_package(XATTR REQUIRED)
find_package(PTHREAD REQUIRED)
find_package(FUSE REQUIRED)
find_program(DEBUILD NAMES debuild)

subdirs(src doc tests)

#
# Project config.
#
if(NOT SYSCONFDIR)
    set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc")
endif(NOT SYSCONFDIR)

set(ROZO_CONFIG_DIR "${SYSCONFDIR}/rozo")

set(EXPORTD_DEFAULT_CONFIG "${ROZO_CONFIG_DIR}/export.conf")

set(STORAGED_DEFAULT_CONFIG "${ROZO_CONFIG_DIR}/storage.conf")

if(NOT DAEMON_PID_DIRECTORY)
    set(DAEMON_PID_DIRECTORY "/var/run")
endif(NOT DAEMON_PID_DIRECTORY)

if(NOT ROZO_RPC_BUFFER_SIZE)
    set(ROZO_RPC_BUFFER_SIZE 0x0)
endif(NOT ROZO_RPC_BUFFER_SIZE)

string(REGEX MATCH "$\\\\/" TRAILING_SLASH "${DAEMON_PID_DIRECTORY}")
if(NOT TRAILING_SLASH)
    set(DAEMON_PID_DIRECTORY "${DAEMON_PID_DIRECTORY}/")
endif(NOT TRAILING_SLASH)

set(ROZO_PACKAGE_FILE_NAME "rozo-${VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")

set(ROZO_SOURCE_PACKAGE_FILE_NAME "rozo-${VERSION}")

configure_file("${PROJECT_SOURCE_DIR}/cmake/uninstall.in" "${PROJECT_BINARY_DIR}/uninstall.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/package.in" "${PROJECT_BINARY_DIR}/package.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/config.h.in" "${PROJECT_BINARY_DIR}/src/config.h")
configure_file("${PROJECT_SOURCE_DIR}/cmake/export.conf.in" "${PROJECT_BINARY_DIR}/src/export.conf.sample")
configure_file("${PROJECT_SOURCE_DIR}/cmake/storage.conf.in" "${PROJECT_BINARY_DIR}/src/storage.conf.sample")
configure_file("${PROJECT_SOURCE_DIR}/doc/rozo.7" "${PROJECT_BINARY_DIR}/doc/rozo.7")
configure_file("${PROJECT_SOURCE_DIR}/doc/rozomount.8" "${PROJECT_BINARY_DIR}/doc/rozomount.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/storaged.8" "${PROJECT_BINARY_DIR}/doc/storaged.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/storage.conf.5" "${PROJECT_BINARY_DIR}/doc/storage.conf.5")
configure_file("${PROJECT_SOURCE_DIR}/doc/exportd.8" "${PROJECT_BINARY_DIR}/doc/exportd.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/export.conf.5" "${PROJECT_BINARY_DIR}/doc/export.conf.5")

#
# Build settings
#
if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING 
  "Choose the type of build, options are: None Debug Release Profile RelWithDebInfo MinSizeRel." 
  FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_C_FLAGS_DEBUG "-g -Wall -DNDEBUGFUNCTION" CACHE STRING
    "Flags used by the C compiler during maintainer builds."
    FORCE)

#Profile build type allows to keep track of the time spent for each action of rozomount (e.g export exchanges, storage exchanges, transform)
#The stat can be viewed in the syslogs when rozomount has been stopped.
set(CMAKE_CXX_FLAGS_PROFILE "-DPROFILE -DNDEBUG")
set(CMAKE_C_FLAGS_PROFILE "-DPROFILE -DNDEBUG")
set(CMAKE_CXX_FLAGS_DBGPROFILE "-DPROFILE")
set(CMAKE_C_FLAGS_DBGPROFILE "-DPROFILE")

add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake")
add_custom_target(package_debian "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/package.cmake")

#
# Packing
#

include(InstallRequiredSystemLibraries)

# Binaries
set(CPACK_GENERATOR "STGZ;TGZ")
set(CPACK_STGZ_COMPONENT_INSTALL ON)
set(CPACK_TGZ_COMPONENT_INSTALL ON)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Rozo filesystem")
set(CPACK_PACKAGE_VENDOR "Fizians SAS")
set(CPACK_PACKAGE_CONTACT "Fizians SAS")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_PACKAGE_FILE_NAME ""${ROZO_PACKAGE_FILE_NAME}"")
set(CPACK_COMPONENTS_ALL client export storage)
set(CPACK_COMPONENT_CLIENT_DISPLAY_NAME "rozomount")
set(CPACK_COMPONENT_EXPORT_DISPLAY_NAME "exportd")
set(CPACK_COMPONENT_STORAGE_DISPLAY_NAME "storaged")
set(CPACK_COMPONENT_CLIENT_DESCRIPTION "rozomount")
set(CPACK_COMPONENT_EXPORT_DESCRIPTION "exportd")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "storaged")

# Sources
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${ROZO_SOURCE_PACKAGE_FILE_NAME}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;${CPACK_SOURCE_PACKAGE_FILE_NAME};tags;/CMakeFiles/;/_CPack_Packages/;/\\\\.hg/;CMakeCache.txt;Makefile;\\\\.vim$;\\\\.swp$;uninstall.cmake$;cmake_install.cmake;CPackConfig.cmake;CPackSourceConfig.cmake;~$;tags;/nbproject/;.hgignore;.dep.inc;/\\\\.settings")
include(CPack)
