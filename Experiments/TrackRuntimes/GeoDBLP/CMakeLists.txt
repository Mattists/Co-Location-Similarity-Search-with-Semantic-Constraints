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
)

set(GEODBLP_WEIGHTED_SUFFIXES
        Unweighted
        Weighted
)

set(GEODBLP_APPROX_SUFFIXES
        UnderApprox
        OverApprox
)

set(GEODBLP_PATHSIM_QUERY_TARGETS)

foreach(grid_resolution ${GEODBLP_GRID_RESOLUTIONS})
    foreach(epsilon ${GEODBLP_EPSILON_VALUES})
        foreach(weighted_suffix ${GEODBLP_WEIGHTED_SUFFIXES})
            foreach(approx_suffix ${GEODBLP_APPROX_SUFFIXES})
                list(APPEND GEODBLP_PATHSIM_QUERY_TARGETS
                        trackPathSimQueryGridResolution${grid_resolution}Epsilon${epsilon}${weighted_suffix}${approx_suffix}
                )
            endforeach()
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
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target}
            PathSim
            DataHandler
            SpatialMetaPathCreator
    )
endforeach()

set(GEODBLP_SPATIALMETAPATH_TARGETS
        trackRuntimesEpsilonSizeGridRes5UnweightedOverApprox
        trackRuntimesEpsilonSizeGridRes5UnweightedUnderApprox
        trackRuntimesEpsilonSizeGridRes5WeightedOverApprox
        trackRuntimesEpsilonSizeGridRes5WeightedUnderApprox
        trackRuntimesEpsilonSizeGridRes7UnweightedOverApprox
        trackRuntimesEpsilonSizeGridRes7UnweightedUnderApprox
        trackRuntimesEpsilonSizeGridRes7WeightedOverApprox
        trackRuntimesEpsilonSizeGridRes7WeightedUnderApprox
        trackRuntimesEpsilonSizeGridRes9UnweightedOverApprox
        trackRuntimesEpsilonSizeGridRes9UnweightedUnderApprox
        trackRuntimesEpsilonSizeGridRes9WeightedOverApprox
        trackRuntimesEpsilonSizeGridRes9WeightedUnderApprox
        trackRuntimesEpsilonSizeEdgeDecompositionUnweighted
        trackRuntimesEpsilonSizeEdgeDecompositionWeighted
)

foreach(target ${GEODBLP_SPATIALMETAPATH_TARGETS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target}
            SpatialMetaPathCreator
    )
endforeach()
