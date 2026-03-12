cmake_minimum_required(VERSION 3.20)
project(pipnn_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIPNN_ENABLE_OPENMP "Enable OpenMP" ON)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

if(PIPNN_ENABLE_OPENMP)
  find_package(OpenMP)
endif()

FetchContent_Declare(
  hnswlib
  GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
  GIT_TAG master
)
FetchContent_MakeAvailable(hnswlib)

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
  FetchContent_Populate(eigen)
endif()

if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${eigen_SOURCE_DIR}"
  )
endif()

add_library(pipnn_core
  src/cli/app.cpp
  src/data/sift_reader.cpp
  src/core/hashprune.cpp
  src/core/rbc.cpp
  src/core/rbc_assignment.cpp
  src/core/leaf_knn.cpp
  src/core/leaf_knn_blocked.cpp
  src/core/pipnn_builder.cpp
  src/search/greedy_beam.cpp
  src/bench/runner.cpp
  src/baseline/hnsw_runner.cpp
)

target_include_directories(pipnn_core PUBLIC src)
target_include_directories(pipnn_core PRIVATE ${hnswlib_SOURCE_DIR})
if(OpenMP_CXX_FOUND)
  target_link_libraries(pipnn_core PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(pipnn_core PUBLIC Eigen3::Eigen)

add_executable(pipnn src/main.cpp)
target_link_libraries(pipnn PRIVATE pipnn_core)

enable_testing()
add_subdirectory(tests)
