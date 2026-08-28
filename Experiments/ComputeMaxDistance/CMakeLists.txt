add_executable(
    h3_max_cell_radius
    h3_max_cell_radius.cpp
)

target_compile_features(
    h3_max_cell_radius
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    h3_max_cell_radius
    PRIVATE
        h3
)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    target_link_libraries(
        h3_max_cell_radius
        PRIVATE
            OpenMP::OpenMP_CXX
    )

    target_compile_definitions(
        h3_max_cell_radius
        PRIVATE
            H3_MAX_CELL_RADIUS_OPENMP_ENABLED=1
    )
else()
    message(WARNING "OpenMP was not found. h3_max_cell_radius can only run resolutions below 6 without OpenMP.")
endif()
