set(YFCC_GRID_RESOLUTIONS
        5
        7
        9
)

set(YFCC_EPSILON_VALUES
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

set(YFCC_WEIGHTED_SUFFIXES
        Unweighted
        Weighted
)

set(YFCC_PATHSIM_QUERY_TARGETS)

foreach(grid_resolution ${YFCC_GRID_RESOLUTIONS})
    foreach(epsilon ${YFCC_EPSILON_VALUES})
        foreach(weighted_suffix ${YFCC_WEIGHTED_SUFFIXES})
            list(APPEND YFCC_PATHSIM_QUERY_TARGETS
                    trackPathSimQueryGridResolution${grid_resolution}Epsilon${epsilon}${weighted_suffix}
            )
        endforeach()
    endforeach()
endforeach()

foreach(epsilon ${YFCC_EPSILON_VALUES})
    foreach(weighted_suffix ${YFCC_WEIGHTED_SUFFIXES})
        list(APPEND YFCC_PATHSIM_QUERY_TARGETS
                trackPathSimQueryEdgeDecompositionEpsilon${epsilon}${weighted_suffix}
        )
    endforeach()
endforeach()

foreach(target ${YFCC_PATHSIM_QUERY_TARGETS})
    set(cmake_target "YFCC_${target}")
    add_executable(${cmake_target} ${target}.cpp)
    set_target_properties(${cmake_target} PROPERTIES OUTPUT_NAME ${target})
    target_link_libraries(${cmake_target}
            PathSim
            DataHandler
            SpatialMetaPathCreator
    )
endforeach()

set(YFCC_SPATIALMETAPATH_TARGETS
        trackRuntimesEpsilonSizeGridRes5Unweighted
        trackRuntimesEpsilonSizeGridRes5Weighted
        trackRuntimesEpsilonSizeGridRes7Unweighted
        trackRuntimesEpsilonSizeGridRes7Weighted
        trackRuntimesEpsilonSizeGridRes9Unweighted
        trackRuntimesEpsilonSizeGridRes9Weighted
        trackRuntimesEpsilonSizeEdgeDecompositionUnweighted
        trackRuntimesEpsilonSizeEdgeDecompositionWeighted
)

foreach(target ${YFCC_SPATIALMETAPATH_TARGETS})
    set(cmake_target "YFCC_${target}")
    add_executable(${cmake_target} ${target}.cpp)
    set_target_properties(${cmake_target} PROPERTIES OUTPUT_NAME ${target})
    target_link_libraries(${cmake_target}
            SpatialMetaPathCreator
    )
endforeach()
