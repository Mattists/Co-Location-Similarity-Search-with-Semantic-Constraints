
add_library(PathSim
        PathSimQuery.hpp
        PathSimQuery.cpp
)

target_link_libraries(PathSim
        PUBLIC
            SpatialMetaPath::project_options
)

target_include_directories(PathSim PUBLIC "${CMAKE_SOURCE_DIR}/src")
