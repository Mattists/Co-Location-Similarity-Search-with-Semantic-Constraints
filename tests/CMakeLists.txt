if(NOT TARGET GTest::gtest_main)
        message(FATAL_ERROR "GoogleTest is required for tests. Configure the root project with BUILD_TESTING=ON.")
endif()

add_executable(EdgeDecompositionUnweightedValidationTest EdgeDecompositionUnweightedValidationTest.cpp)
target_link_libraries(EdgeDecompositionUnweightedValidationTest
        PRIVATE
        PathSim
        SpatialMetaPathCreator
        DataHandler
        GTest::gtest_main
)
target_compile_definitions(EdgeDecompositionUnweightedValidationTest
        PRIVATE
        SPATIALMETAPATH_DATASETS_DIR="${CMAKE_SOURCE_DIR}/Datasets"
)

include(GoogleTest)
gtest_discover_tests(EdgeDecompositionUnweightedValidationTest)
