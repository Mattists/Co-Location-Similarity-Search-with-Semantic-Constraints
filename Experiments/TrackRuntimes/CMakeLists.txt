add_executable(countSpatialNeighbors countSpatialNeighbors.cpp)

target_link_libraries(countSpatialNeighbors
        DataHandler
        SpatialMetaPathCreator
)

add_subdirectory(Foursquare)
add_subdirectory(GeoDBLP)
add_subdirectory(YFCC)
