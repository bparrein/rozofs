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
set(MINOR "2")
set(STATUS "0") # 0: alpha, 1: beta, 2: release candidate, 3: public release  
set(REVISION "6")

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

set(VERSION "${MAJOR}.${MINOR}.${STATUS}.${REVISION}")
set(CONF_DIR "${CMAKE_INSTALL_PREFIX}/etc/rozo")
set(EXPORTD_DEFAULT_CONFIG "${CONF_DIR}/export.conf")
set(STORAGED_DEFAULT_CONFIG "${CONF_DIR}/storage.conf")

subdirs(src doc tests)

configure_file("${PROJECT_SOURCE_DIR}/cmake/uninstall.in" "${PROJECT_BINARY_DIR}/uninstall.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/config.h.in" "${PROJECT_BINARY_DIR}/src/config.h")
configure_file("${PROJECT_SOURCE_DIR}/cmake/export.conf.in" "${PROJECT_BINARY_DIR}/src/export.conf.sample")
configure_file("${PROJECT_SOURCE_DIR}/cmake/storage.conf.in" "${PROJECT_BINARY_DIR}/src/storage.conf.sample")
configure_file("${PROJECT_SOURCE_DIR}/doc/rozo.7" "${PROJECT_BINARY_DIR}/doc/rozo.7")
configure_file("${PROJECT_SOURCE_DIR}/doc/rozofs.8" "${PROJECT_BINARY_DIR}/doc/rozofs.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/storaged.8" "${PROJECT_BINARY_DIR}/doc/storaged.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/storage.conf.5" "${PROJECT_BINARY_DIR}/doc/storage.conf.5")
configure_file("${PROJECT_SOURCE_DIR}/doc/exportd.8" "${PROJECT_BINARY_DIR}/doc/exportd.8")
configure_file("${PROJECT_SOURCE_DIR}/doc/export.conf.5" "${PROJECT_BINARY_DIR}/doc/export.conf.5")

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/cmake")

#
# Find packages
#
find_package(UUID REQUIRED)
find_package(CONFIG REQUIRED)
find_package(XATTR REQUIRED)
find_package(PTHREAD REQUIRED)
find_package(FUSE REQUIRED)

#
# Add special targets
#
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake")

#
# Packing
#
include(InstallRequiredSystemLibraries)

# Binaries
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Rozo cloud filesystem")
set(CPACK_PACKAGE_VENDOR "Fizians S.A.S")
set(CPACK_PACKAGE_CONTACT "Fizians S.A.S")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${REVISION}")
set(CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_PACKAGE_FILE_NAME "rozo-${VERSION}-${CMAKE_SYSTEM_NAME}")

#set(CPACK_GENERATOR "TGZ;DEB")

# Sources
set(CPACK_SOURCE_PACKAGE_FILE_NAME "rozo-${VERSION}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;${CPACK_SOURCE_PACKAGE_FILE_NAME};tags;/CMakeFiles/;/_CPack_Packages/;/\\\\.hg/;CMakeCache.txt;Makefile;\\\\.vim$;\\\\.swp$;uninstall.cmake$;cmake_install.cmake;CPackConfig.cmake;CPackSourceConfig.cmake;~$;tags;/nbproject/;.hgignore;.dep.inc")
# Deb
#set(CPACK_DEBIAN_PACKAGE_SECTION "net")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "portmap (>=6.0.0-1), libc6 (>= 2.11.1-0), libuuid1 (>=2.17.2-0), libconfig8(>=1.3.2-2), libattr1(>=1:2.4.44-1), libfuse2(>=2.8.1-1.1), fuse-utils(>=2.8.1-1.1)")
include(CPack)

