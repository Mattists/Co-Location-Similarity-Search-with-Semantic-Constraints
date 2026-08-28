set(APPROXIMATION_QUALITY_TARGETS
        compareApproximationQualityFoursquareRes5
        compareApproximationQualityFoursquareRes6
        compareApproximationQualityFoursquareRes7
        compareApproximationQualityFoursquareRes8
        compareApproximationQualityFoursquareRes9
        compareApproximationQualityGeoDBLPRes5
        compareApproximationQualityGeoDBLPRes6
        compareApproximationQualityGeoDBLPRes7
        compareApproximationQualityGeoDBLPRes8
        compareApproximationQualityGeoDBLPRes9
        compareApproximationQualityYFCCRes5
        compareApproximationQualityYFCCRes6
        compareApproximationQualityYFCCRes7
        compareApproximationQualityYFCCRes8
        compareApproximationQualityYFCCRes9
)

foreach(target ${APPROXIMATION_QUALITY_TARGETS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target}
            PathSim
            SpatialMetaPathCreator
    )
endforeach()
