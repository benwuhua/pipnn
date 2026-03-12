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

add_executable(test_quality_evidence test_quality_evidence.cpp)
target_compile_definitions(test_quality_evidence PRIVATE PIPNN_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME quality_evidence COMMAND test_quality_evidence)

add_executable(test_mutation_evidence test_mutation_evidence.cpp)
target_compile_definitions(test_mutation_evidence PRIVATE PIPNN_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME mutation_evidence COMMAND test_mutation_evidence)

add_executable(test_remote_toolchain test_remote_toolchain.cpp)
target_compile_definitions(test_remote_toolchain PRIVATE PIPNN_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME remote_toolchain COMMAND test_remote_toolchain)

add_executable(test_mutation_report_aggregator test_mutation_report_aggregator.cpp)
target_compile_definitions(test_mutation_report_aggregator PRIVATE PIPNN_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME mutation_report_aggregator COMMAND test_mutation_report_aggregator)

add_executable(test_real_test_harness test_real_test_harness.cpp)
target_compile_definitions(test_real_test_harness PRIVATE PIPNN_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME real_test_harness COMMAND test_real_test_harness)
