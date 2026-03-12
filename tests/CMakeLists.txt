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
