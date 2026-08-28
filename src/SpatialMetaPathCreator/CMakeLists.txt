
add_library(SpatialMetaPathCreator
	        SpatialMetaPathEdgeDecomposition.cpp
            SpatialMetaPathEdgeDecomposition.hpp
	        SpatialMetaPathGrid.cpp
            SpatialMetaPathGrid.hpp
	        SpatialMetaPathHelper.cpp
	        SpatialMetaPathHelper.hpp
)

target_link_libraries(SpatialMetaPathCreator
    PUBLIC
        RTree
        DataHandler
        SpatialMetaPath::project_options
    PRIVATE
        h3
)

target_include_directories(SpatialMetaPathCreator PUBLIC "${CMAKE_SOURCE_DIR}/src")
