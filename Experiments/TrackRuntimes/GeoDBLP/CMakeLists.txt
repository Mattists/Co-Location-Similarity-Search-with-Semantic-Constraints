set(GEODBLP_GRID_RESOLUTIONS
        5
        7
        9
)

set(GEODBLP_EPSILON_VALUES
        100
        200
        500
        1000
        2000
        5000
        10000
        20000
        50000
        75000
)

set(GEODBLP_WEIGHTED_SUFFIXES
        Unweighted
        Weighted
)

set(GEODBLP_PATHSIM_QUERY_TARGETS)

foreach(grid_resolution ${GEODBLP_GRID_RESOLUTIONS})
    foreach(epsilon ${GEODBLP_EPSILON_VALUES})
        foreach(weighted_suffix ${GEODBLP_WEIGHTED_SUFFIXES})
            list(APPEND GEODBLP_PATHSIM_QUERY_TARGETS
                    trackPathSimQueryGridResolution${grid_resolution}Epsilon${epsilon}${weighted_suffix}
            )
        endforeach()
    endforeach()
endforeach()

foreach(epsilon ${GEODBLP_EPSILON_VALUES})
    foreach(weighted_suffix ${GEODBLP_WEIGHTED_SUFFIXES})
        list(APPEND GEODBLP_PATHSIM_QUERY_TARGETS
                trackPathSimQueryEdgeDecompositionEpsilon${epsilon}${weighted_suffix}
        )
    endforeach()
endforeach()

foreach(target ${GEODBLP_PATHSIM_QUERY_TARGETS})
    set(cmake_target "GEODBLP_${target}")
    add_executable(${cmake_target} ${target}.cpp)
    set_target_properties(${cmake_target} PROPERTIES OUTPUT_NAME ${target})
    target_link_libraries(${cmake_target}
            PathSim
            DataHandler
            SpatialMetaPathCreator
    )
endforeach()

set(GEODBLP_SPATIALMETAPATH_TARGETS
        trackRuntimesEpsilonSizeGridRes5Unweighted
        trackRuntimesEpsilonSizeGridRes5Weighted
        trackRuntimesEpsilonSizeGridRes7Unweighted
        trackRuntimesEpsilonSizeGridRes7Weighted
        trackRuntimesEpsilonSizeGridRes9Unweighted
        trackRuntimesEpsilonSizeGridRes9Weighted
        trackRuntimesEpsilonSizeEdgeDecompositionUnweighted
        trackRuntimesEpsilonSizeEdgeDecompositionWeighted
)

foreach(target ${GEODBLP_SPATIALMETAPATH_TARGETS})
    set(cmake_target "GEODBLP_${target}")
    add_executable(${cmake_target} ${target}.cpp)
    set_target_properties(${cmake_target} PROPERTIES OUTPUT_NAME ${target})
    target_link_libraries(${cmake_target}
            SpatialMetaPathCreator
    )
endforeach()
