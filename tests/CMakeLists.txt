add_executable(test_hashprune test_hashprune.cpp)
target_link_libraries(test_hashprune PRIVATE pipnn_core)
add_test(NAME hashprune COMMAND test_hashprune)

add_executable(test_sift_reader test_sift_reader.cpp)
target_link_libraries(test_sift_reader PRIVATE pipnn_core)
add_test(NAME sift_reader COMMAND test_sift_reader)

add_executable(test_rbc test_rbc.cpp)
target_link_libraries(test_rbc PRIVATE pipnn_core)
add_test(NAME rbc COMMAND test_rbc)

add_executable(test_leaf_knn test_leaf_knn.cpp)
target_link_libraries(test_leaf_knn PRIVATE pipnn_core)
add_test(NAME leaf_knn COMMAND test_leaf_knn)

add_executable(test_pipnn_integration test_pipnn_integration.cpp)
target_link_libraries(test_pipnn_integration PRIVATE pipnn_core)
add_test(NAME pipnn_integration COMMAND test_pipnn_integration)

add_executable(test_hnsw_runner test_hnsw_runner.cpp)
target_link_libraries(test_hnsw_runner PRIVATE pipnn_core)
add_test(NAME hnsw_runner COMMAND test_hnsw_runner)

add_executable(test_cli_app test_cli_app.cpp)
target_link_libraries(test_cli_app PRIVATE pipnn_core)
add_test(NAME cli_app COMMAND test_cli_app)

add_executable(test_cli test_cli.cpp)
target_compile_definitions(test_cli PRIVATE PIPNN_BIN_PATH="$<TARGET_FILE:pipnn>")
add_dependencies(test_cli pipnn)
add_test(NAME cli COMMAND test_cli)
