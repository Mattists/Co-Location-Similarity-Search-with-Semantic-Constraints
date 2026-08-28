add_executable(analyzeGeoDBLPEdgeDecomposition analyzeGeoDBLPEdgeDecomposition.cpp)
add_executable(analyzeGeoDBLPGrid analyzeGeoDBLPGrid.cpp)

add_executable(analyzeYFCCEdgeDecomposition analyzeYFCCEdgeDecomposition.cpp)
add_executable(analyzeYFCCGrid analyzeYFCCGrid.cpp)

add_executable(analyzeFoursquareEdgeDecomposition analyzeFoursquareEdgeDecomposition.cpp)
add_executable(analyzeFoursquareGrid analyzeFoursquareGrid.cpp)

set(ANALYZE_TARGETS
        analyzeGeoDBLPEdgeDecomposition
        analyzeGeoDBLPGrid
        analyzeYFCCEdgeDecomposition
        analyzeYFCCGrid
        analyzeFoursquareEdgeDecomposition
        analyzeFoursquareGrid
)

foreach(target ${ANALYZE_TARGETS})
    target_link_libraries(${target}
            PathSim
            DataHandler
            SpatialMetaPathCreator
    )
endforeach()
