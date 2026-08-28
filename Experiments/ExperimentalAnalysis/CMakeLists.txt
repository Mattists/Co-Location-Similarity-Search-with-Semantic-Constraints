add_executable(computeFoursquareUserPathSimScoresUnweighted
        computeFoursquareUserPathSimScoresUnweighted.cpp
        FoursquareUserPathSimScores.hpp)

add_executable(computeFoursquareUserPathSimScoresWeighted
        computeFoursquareUserPathSimScoresWeighted.cpp
        FoursquareUserPathSimScores.hpp)


set(EXPERIMENTAL_ANALYSIS_TARGETS
        computeFoursquareUserPathSimScoresUnweighted
        computeFoursquareUserPathSimScoresWeighted
)

foreach(target ${EXPERIMENTAL_ANALYSIS_TARGETS})
    target_link_libraries(${target}
            DataHandler
            PathSim
            SpatialMetaPathCreator
    )
endforeach()
