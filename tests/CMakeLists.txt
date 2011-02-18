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

add_executable(test_distribution
    ../src/distribution.h
    test_distribution.c
)

add_executable(test_hash_table
    ../src/hash_table.h
    ../src/hash_table.c
    test_hash_table.c
)

add_executable(test_pool
    ../src/rpc_client.h
    ../src/rpc_client.c
    ../src/hash_table.h
    ../src/hash_table.c
    ../src/pool.h
    ../src/pool.c
    test_pool.c
)
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
    ../src/distribution.h
    ../src/hash_table.h
    ../src/hash_table.c
    ../src/pool.h
    ../src/pool.c
    ../src/rozo_client.h
    ../src/rozo_client.c
    sendfile.c
)
target_link_libraries(sendfile ${UUID_LIBRARY})

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
    ../src/distribution.h
    ../src/hash_table.h
    ../src/hash_table.c
    ../src/pool.h
    ../src/pool.c
    ../src/rozo_client.h
    ../src/rozo_client.c
    retrievefile.c
)
target_link_libraries(retrievefile ${UUID_LIBRARY})

