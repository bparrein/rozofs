include_directories("${CMAKE_SOURCE_DIR}/src" ${BERKELEY_DB_INCLUDE_DIR})

add_executable(test_list
    ../src/list.h
    test_list.c
)

add_executable(test_transform
    ../src/rozo.h
    ../src/rozo.c
    ../src/transform.h
    ../src/transform.c
    test_transform.c
)

add_executable(test_storage
    ../src/rozo.h
    ../src/rozo.c
    ../src/storage.h
    ../src/storage.c
    test_storage.c
)
target_link_libraries(test_storage ${UUID_LIBRARY})

add_executable(test_volume
    ../src/rozo.h
    ../src/rozo.c
    ../src/storage.h
    ../src/storage.c
    ../src/storage_proto.h
    ../src/storage_proto_xdr.c
    ../src/storage_proto_clt.c
    ../src/volume.h
    ../src/volume.c
    test_volume.c
)
target_link_libraries(test_volume ${PTHREAD_LIBRARY} ${UUID_LIBRARY})

add_executable(test_export_config
    ../src/export_config.h
    ../src/export_config.c
    test_export_config.c
)
target_link_libraries(test_export_config ${CONFIG_LIBRARY} ${UUID_LIBRARY})

add_executable(test_storage_config
    ../src/storage_config.h
    ../src/storage_config.c
    test_storage_config.c
)
target_link_libraries(test_storage_config ${CONFIG_LIBRARY} ${UUID_LIBRARY})


add_executable(transform_throughput
    ../src/rozo.h
    ../src/rozo.c
    ../src/transform.h
    ../src/transform.c
    transform_throughput.c
)

add_executable(sendfile
    ../src/rozo.h
    ../src/rozo.c
    ../src/transform.h
    ../src/transform.c
    ../src/storage_proto.h
    ../src/storage_proto_xdr.c
    ../src/storage_proto_clt.c
    ../src/export_proto.h
    ../src/export_proto_xdr.c
    ../src/export_proto_clt.c
    ../src/rpc_client.h
    ../src/rpc_client.c
    ../src/rozo_client.h
    ../src/rozo_client.c
    sendfile.c
)
target_link_libraries(sendfile ${UUID_LIBRARY})

#add_executable(sendfile_multiple
#    ../src/rozo.h
#    ../src/rozo.c
#    ../src/transform.h
#    ../src/transform.c
#    ../src/ps_proto.h
#    ../src/ps_proto_xdr.c
#    ../src/ps_proto_clt.c
#    ../src/mfs_proto.h
#    ../src/mfs_proto_xdr.c
#    ../src/mfs_proto_clt.c
#    ../src/rpc_client.h
#    ../src/rpc_client.c
#    ../src/api.h
#    ../src/api.c
#    sendfile_multiple.c
#)
#target_link_libraries(sendfile_multiple ${UUID_LIBRARY})

add_executable(retrievefile
    ../src/rozo.h
    ../src/rozo.c
    ../src/transform.h
    ../src/transform.c
    ../src/storage_proto.h
    ../src/storage_proto_xdr.c
    ../src/storage_proto_clt.c
    ../src/export_proto.h
    ../src/export_proto_xdr.c
    ../src/export_proto_clt.c
    ../src/rpc_client.h
    ../src/rpc_client.c
    ../src/rozo_client.h
    ../src/rozo_client.c
    retrievefile.c
)
target_link_libraries(retrievefile ${UUID_LIBRARY})

add_executable(test_fs_operations test_fs_operations.c)
target_link_libraries(test_fs_operations ${UUID_LIBRARY})

add_custom_target(copy_run_acceptance_tests ALL command ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/run_acceptance_tests.sh  ${CMAKE_CURRENT_BINARY_DIR}/run_acceptance_tests.sh)

