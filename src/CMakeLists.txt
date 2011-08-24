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

add_definitions(-DD_REENTRANT -D_FILE_OFFSET_BITS=64)

include_directories(${CMAKE_CURRENT_BINARY_DIR}) # for config.h

include_directories(${PTHREAD_INCLUDE_DIR})

add_executable(storaged
    config.h
    rozo.h
    rozo.c
    log.h
    list.h
    xmalloc.h
    xmalloc.c
    htable.h
    htable.c
    storage.h
    storage.c
    sproto.h
    sprotoxdr.c
    sprotosvc.c
    sproto.c
    daemon.h
    daemon.c
    storaged.c
)
target_link_libraries(storaged ${XATTR_LIBRARY} ${UUID_LIBRARY} ${CONFIG_LIBRARY})

add_executable(exportd
    config.h
    rozo.h
    rozo.c
    log.h
    list.h
    dist.h
    xmalloc.h
    xmalloc.c
    htable.h
    htable.c
    daemon.h
    daemon.c
    volume.h
    volume.c
    export.h
    export.c
    eproto.h
    eprotoxdr.c
    eprotosvc.c
    eproto.c
    rpcclt.h
    rpcclt.c
    sproto.h
    sprotoxdr.c
    sprotoclt.c
    storageclt.c
    exportd.c
)
target_link_libraries(exportd ${XATTR_LIBRARY} ${PTHREAD_LIBRARY} ${UUID_LIBRARY} ${CONFIG_LIBRARY})

add_executable(rozomount
    config.h
    rozo.h
    rozo.c
    log.h
    xmalloc.h
    xmalloc.c
    transform.h
    transform.c
    dist.h
    htable.h
    htable.c
    rpcclt.h
    rpcclt.c    
    sproto.h
    sprotoxdr.c
    sprotoclt.c
    storageclt.c
    eproto.h
    eprotoxdr.c
    profile.h
    profile.c
    eprotoclt.c
    exportclt.c
    file.h
    file.c
    rozomount.c
)
#pthread need by fuse 
target_link_libraries(rozomount ${PTHREAD_LIBRARY} ${FUSE_LIBRARY} ${UUID_LIBRARY})

# Install
install(CODE "EXECUTE_PROCESS (COMMAND mkdir -p ${ROZO_CONFIG_DIR})" COMPONENT storage)
install(CODE "EXECUTE_PROCESS (COMMAND mkdir -p ${ROZO_CONFIG_DIR})" COMPONENT export)

install(TARGETS storaged
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT storage
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/storage.conf.sample DESTINATION ${ROZO_CONFIG_DIR} COMPONENT storage)

install(TARGETS exportd
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT export
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/export.conf.sample DESTINATION ${ROZO_CONFIG_DIR} COMPONENT export)

install(TARGETS rozomount
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT client
)
