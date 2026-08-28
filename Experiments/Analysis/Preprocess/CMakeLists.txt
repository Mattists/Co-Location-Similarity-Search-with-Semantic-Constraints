add_executable(preprocessGeoDBLP preprocessGeoDBLP.cpp)
add_executable(preprocessYFCC preprocessYFCC.cpp)
add_executable(preprocessFoursquare preprocessFoursquare.cpp)

set(PREPROCESS_TARGETS
        preprocessGeoDBLP
        preprocessYFCC
        preprocessFoursquare
)

foreach(target ${PREPROCESS_TARGETS})
    target_link_libraries(${target}
            DataHandler
    )
endforeach()
