
add_library(DataHandler
        MatrixIO.hpp
        MatrixIO.cpp
        Preprocessor.hpp
        Preprocessor.cpp
        utils.hpp
)

target_link_libraries(DataHandler
        PUBLIC
            SpatialMetaPath::project_options
)

target_include_directories(DataHandler PUBLIC "${CMAKE_SOURCE_DIR}/src")
