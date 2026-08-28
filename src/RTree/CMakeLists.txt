cmake_minimum_required(VERSION 3.14)

project(RTree)

add_library(RTree INTERFACE)
target_include_directories(RTree INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

option(SPATIALMETAPATH_BUILD_RTREE_TESTS "Build the vendored RTree unit tests." OFF)

if(SPATIALMETAPATH_BUILD_RTREE_TESTS)
  if(NOT TARGET GTest::gtest_main OR NOT TARGET GTest::gmock_main)
    find_package(GTest CONFIG QUIET)
  endif()

  if(NOT TARGET GTest::gtest_main OR NOT TARGET GTest::gmock_main)
    message(FATAL_ERROR "RTree tests require GoogleTest. Configure the root project with BUILD_TESTING=ON or provide a GTest package.")
  endif()

  add_subdirectory(tests)
endif()
