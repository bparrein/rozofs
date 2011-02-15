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
    log.h
    rozo.h
    rozo.c
    daemon.h
    daemon.c
    storage.h
    storage.c
    storage_config.h
    storage_config.c
    storage_proto.h
    storage_proto_xdr.c
    storage_proto_svc.c
    storage_proto.c
    storaged.c
)
target_link_libraries(storaged ${XATTR_LIBRARY} ${UUID_LIBRARY} ${CONFIG_LIBRARY})

add_executable(exportd
    rozo.h
    rozo.c
    log.h
    daemon.h
    daemon.c
    queue_log.h
    queue_log.c
    file_log.h
    file_log.c
    volume.h
    volume.c
    distribution.h
    vfs.h
    vfs.c
    export_config.h
    export_config.c
    export_proto.h
    export_proto_xdr.c
    export_proto_svc.c
    export_proto.c
    storage_proto.h
    storage_proto_xdr.c
    storage_proto_clt.c
    exportd.c
)
target_link_libraries(exportd ${XATTR_LIBRARY} ${PTHREAD_LIBRARY} ${UUID_LIBRARY} ${CONFIG_LIBRARY})

add_executable(rozofs
    log.h
    transform.h
    transform.c
    rozo.h
    rozo.c
    export_proto.h
    export_proto_xdr.c
    export_proto_clt.c
    storage_proto.h
    storage_proto_xdr.c
    storage_proto_clt.c
    rpc_client.h
    rpc_client.c
    rozo_client.h
    rozo_client.c
    rozofs.c
)
#pthread need by fuse 
target_link_libraries(rozofs ${PTHREAD_LIBRARY} ${FUSE_LIBRARY} ${UUID_LIBRARY})

# Install
install(CODE "EXECUTE_PROCESS (COMMAND mkdir -p ${CONF_DIR})" COMPONENT storage)
install(CODE "EXECUTE_PROCESS (COMMAND mkdir -p ${CONF_DIR})" COMPONENT export)

install(TARGETS storaged
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT storage
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/storage.conf.sample DESTINATION etc/rozo COMPONENT storage)

install(TARGETS exportd
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT storage
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/export.conf.sample DESTINATION etc/rozo COMPONENT export)

install(TARGETS rozofs
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT client
)

