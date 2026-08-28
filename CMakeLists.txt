cmake_minimum_required(VERSION 3.16)
project(SpatialMetaPath VERSION 0.1.0)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SPATIALMETAPATH_ENABLE_NATIVE_OPTIMIZATIONS "Optimize for the build host CPU using -march=native in Release builds." OFF)
option(SPATIALMETAPATH_ENABLE_IPO "Enable interprocedural optimization/link-time optimization for Release builds when supported." OFF)
option(BUILD_TESTING "Build project tests." OFF)
option(SPATIALMETAPATH_BUILD_RTREE_TESTS "Build the vendored RTree unit tests." OFF)

include(CTest)

add_library(SpatialMetaPath_project_options INTERFACE)
add_library(SpatialMetaPath::project_options ALIAS SpatialMetaPath_project_options)

target_compile_options(SpatialMetaPath_project_options INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wno-deprecated-enum-enum-conversion>
)

if(SPATIALMETAPATH_ENABLE_NATIVE_OPTIMIZATIONS)
    target_compile_options(SpatialMetaPath_project_options INTERFACE
            $<$<AND:$<CONFIG:Release>,$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>>:-march=native>
    )
endif()

if(SPATIALMETAPATH_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT SPATIALMETAPATH_IPO_SUPPORTED OUTPUT SPATIALMETAPATH_IPO_ERROR LANGUAGES CXX)
    if(SPATIALMETAPATH_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    else()
        message(WARNING "IPO/LTO is not supported by this toolchain: ${SPATIALMETAPATH_IPO_ERROR}")
    endif()
endif()

# sparse matrix library
find_package(Eigen3 REQUIRED)
if(TARGET Eigen3::Eigen)
    target_link_libraries(SpatialMetaPath_project_options INTERFACE Eigen3::Eigen)
elseif(EIGEN3_INCLUDE_DIR)
    target_include_directories(SpatialMetaPath_project_options INTERFACE "${EIGEN3_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Please point the environment variable EIGEN3_INCLUDE_DIR to the include directory of your Eigen3 installation.")
endif()


# =========================
# H3
# =========================
include(FetchContent)

FetchContent_Declare(
    h3
    GIT_REPOSITORY https://github.com/uber/h3.git
    GIT_TAG v4.2.1
)

FetchContent_MakeAvailable(h3)

if(BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    FetchContent_MakeAvailable(googletest)
endif()

add_subdirectory(src)
add_subdirectory(Experiments)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
