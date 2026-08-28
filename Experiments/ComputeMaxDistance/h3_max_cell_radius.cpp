#include <h3api.h>

extern "C" {
#include <iterators.h>
}

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

constexpr int MIN_OPENMP_RESOLUTION = 6;
constexpr long double EARTH_RADIUS_M = 6'371'008.8L;
constexpr long double PI =
        3.141592653589793238462643383279502884L;
constexpr long double ANGLE_EPSILON = 1.0e-18L;

struct Vec3 {
    long double x;
    long double y;
    long double z;
};

struct Maximum {
    long double distanceM = -1.0L;
    H3Index cell = 0;
    int numBoundaryVertices = 0;
};

struct Statistics {
    std::uint64_t cellsChecked = 0;
    Maximum maximumVertexDistance;
    Maximum maximumBoundaryDistance;
};

[[nodiscard]]
long double clampUnit(const long double value) {
    return std::clamp(value, -1.0L, 1.0L);
}

[[nodiscard]]
Vec3 operator+(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
}

[[nodiscard]]
Vec3 operator-(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z,
    };
}

[[nodiscard]]
Vec3 operator*(const Vec3& vector, const long double factor) {
    return {
        vector.x * factor,
        vector.y * factor,
        vector.z * factor,
    };
}

[[nodiscard]]
Vec3 operator/(const Vec3& vector, const long double divisor) {
    return {
        vector.x / divisor,
        vector.y / divisor,
        vector.z / divisor,
    };
}

[[nodiscard]]
long double dot(const Vec3& lhs, const Vec3& rhs) {
    return lhs.x * rhs.x
         + lhs.y * rhs.y
         + lhs.z * rhs.z;
}

[[nodiscard]]
Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]]
long double norm(const Vec3& vector) {
    return std::sqrt(dot(vector, vector));
}

[[nodiscard]]
Vec3 toUnitVector(const LatLng& coordinate) {
    const long double latitude =
            static_cast<long double>(coordinate.lat);
    const long double longitude =
            static_cast<long double>(coordinate.lng);

    const long double cosLatitude = std::cos(latitude);

    return {
        cosLatitude * std::cos(longitude),
        cosLatitude * std::sin(longitude),
        std::sin(latitude),
    };
}

[[nodiscard]]
long double centralAngle(
        const Vec3& first,
        const Vec3& second) {

    const long double sine = norm(cross(first, second));
    const long double cosine = clampUnit(dot(first, second));

    return std::atan2(sine, cosine);
}

[[nodiscard]]
long double greatCircleDistanceMeters(
        const Vec3& first,
        const Vec3& second) {

    return EARTH_RADIUS_M * centralAngle(first, second);
}

[[nodiscard]]
long double maximumAngleOnMinorArc(
        const Vec3& center,
        const Vec3& arcStart,
        const Vec3& arcEnd) {

    const long double theta =
            centralAngle(arcStart, arcEnd);

    if (theta <= ANGLE_EPSILON) {
        return centralAngle(center, arcStart);
    }

    const long double cosTheta =
            clampUnit(dot(arcStart, arcEnd));
    const long double sinTheta = std::sin(theta);

    if (std::abs(sinTheta) <= ANGLE_EPSILON) {
        return std::max(
            centralAngle(center, arcStart),
            centralAngle(center, arcEnd)
        );
    }

    const Vec3 tangent =
            (arcEnd - arcStart * cosTheta) / sinTheta;

    const long double coefficientCos = dot(center, arcStart);
    const long double coefficientSin = dot(center, tangent);

    const auto projection = [&](const long double s) {
        return coefficientCos * std::cos(s)
             + coefficientSin * std::sin(s);
    };

    long double minimumProjection = std::min(
        projection(0.0L),
        projection(theta)
    );

    const long double baseStationaryPoint =
            std::atan2(coefficientSin, coefficientCos);

    for (int k = -2; k <= 2; ++k) {
        const long double candidate =
                baseStationaryPoint
                + static_cast<long double>(k) * PI;

        if (candidate > 0.0L && candidate < theta) {
            minimumProjection = std::min(
                minimumProjection,
                projection(candidate)
            );
        }
    }

    const long double cosine =
            clampUnit(minimumProjection);
    const long double sine =
            std::sqrt(std::max(
                0.0L,
                1.0L - cosine * cosine
            ));

    return std::atan2(sine, cosine);
}

void updateMaximum(
        Maximum& maximum,
        const long double distanceM,
        const H3Index cell,
        const int numBoundaryVertices) {

    if (distanceM > maximum.distanceM) {
        maximum.distanceM = distanceM;
        maximum.cell = cell;
        maximum.numBoundaryVertices = numBoundaryVertices;
    }
}

H3Error evaluateCell(
        const H3Index cell,
        Statistics& statistics) {

    LatLng centerLatLng{};
    CellBoundary boundary{};

    H3Error error = cellToLatLng(cell, &centerLatLng);

    if (error != E_SUCCESS) {
        return error;
    }

    error = cellToBoundary(cell, &boundary);

    if (error != E_SUCCESS) {
        return error;
    }

    if (boundary.numVerts <= 0) {
        return E_FAILED;
    }

    const Vec3 center = toUnitVector(centerLatLng);

    for (int vertexIndex = 0;
         vertexIndex < boundary.numVerts;
         ++vertexIndex) {

        const Vec3 vertex =
                toUnitVector(boundary.verts[vertexIndex]);

        const long double distanceM =
                greatCircleDistanceMeters(center, vertex);

        updateMaximum(
            statistics.maximumVertexDistance,
            distanceM,
            cell,
            boundary.numVerts
        );
    }

    for (int startIndex = 0;
         startIndex < boundary.numVerts;
         ++startIndex) {

        const int endIndex =
                (startIndex + 1) % boundary.numVerts;

        const Vec3 start =
                toUnitVector(boundary.verts[startIndex]);
        const Vec3 end =
                toUnitVector(boundary.verts[endIndex]);

        const long double maximumAngle =
                maximumAngleOnMinorArc(center, start, end);

        const long double distanceM =
                EARTH_RADIUS_M * maximumAngle;

        updateMaximum(
            statistics.maximumBoundaryDistance,
            distanceM,
            cell,
            boundary.numVerts
        );
    }

    ++statistics.cellsChecked;

    return E_SUCCESS;
}

void mergeMaximum(
        Maximum& target,
        const Maximum& source) {

    if (source.distanceM > target.distanceM) {
        target = source;
    }
}

void mergeStatistics(
        Statistics& target,
        const Statistics& source) {

    target.cellsChecked += source.cellsChecked;

    mergeMaximum(
        target.maximumVertexDistance,
        source.maximumVertexDistance
    );

    mergeMaximum(
        target.maximumBoundaryDistance,
        source.maximumBoundaryDistance
    );
}

[[nodiscard]]
std::string cellToHexString(const H3Index cell) {
    std::ostringstream stream;

    stream
        << std::hex
        << std::nouppercase
        << static_cast<std::uint64_t>(cell);

    return stream.str();
}

[[nodiscard]]
int requestedThreadCount(const int resolution) {
    if (resolution < MIN_OPENMP_RESOLUTION) {
        return 1;
    }

#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

void requireOpenMpForResolution(const int resolution) {
#ifndef _OPENMP
    if (resolution >= MIN_OPENMP_RESOLUTION) {
        throw std::runtime_error(
            "OpenMP support is required for H3 resolution 6 and above."
        );
    }
#else
    (void)resolution;
#endif
}

[[nodiscard]]
Statistics evaluateResolution(const int resolution) {
    if (resolution < 0 || resolution > 15) {
        throw std::invalid_argument(
            "H3 resolution must be between 0 and 15."
        );
    }

    const int baseCellCount = res0CellCount();

    std::vector<H3Index> baseCells(
        static_cast<std::size_t>(baseCellCount)
    );

    const H3Error baseCellError =
            getRes0Cells(baseCells.data());

    if (baseCellError != E_SUCCESS) {
        throw std::runtime_error(
            "getRes0Cells failed with H3 error "
            + std::to_string(baseCellError)
        );
    }

    Statistics globalStatistics;
    std::atomic<bool> failed{false};
    std::string errorMessage;

#pragma omp parallel if(resolution >= MIN_OPENMP_RESOLUTION)
    {
        Statistics localStatistics;

#pragma omp for schedule(dynamic, 1)
        for (int baseCellIndex = 0;
             baseCellIndex < baseCellCount;
             ++baseCellIndex) {

            if (failed.load()) {
                continue;
            }

            const H3Index baseCell =
                    baseCells[
                        static_cast<std::size_t>(baseCellIndex)
                    ];

            if (resolution == 0) {
                const H3Error error =
                        evaluateCell(baseCell, localStatistics);

                if (error != E_SUCCESS) {
                    failed.store(true);

#pragma omp critical(h3_error_message)
                    {
                        if (errorMessage.empty()) {
                            errorMessage =
                                "evaluateCell failed with H3 error "
                                + std::to_string(error);
                        }
                    }
                }

                continue;
            }

            for (IterCellsChildren iterator =
                        iterInitParent(baseCell, resolution);
                 iterator.h != H3_NULL;
                 iterStepChild(&iterator)) {

                if (failed.load()) {
                    continue;
                }

                const H3Error error =
                        evaluateCell(iterator.h, localStatistics);

                if (error != E_SUCCESS) {
                    failed.store(true);

#pragma omp critical(h3_error_message)
                    {
                        if (errorMessage.empty()) {
                            errorMessage =
                                "evaluateCell failed for H3 cell "
                                + cellToHexString(iterator.h)
                                + " with H3 error "
                                + std::to_string(error);
                        }
                    }

                    break;
                }
            }
        }

#pragma omp critical(h3_result_merge)
        {
            mergeStatistics(
                globalStatistics,
                localStatistics
            );
        }
    }

    if (failed.load()) {
        throw std::runtime_error(errorMessage);
    }

    int64_t expectedCellCount = 0;

    const H3Error countError =
            getNumCells(resolution, &expectedCellCount);

    if (countError != E_SUCCESS) {
        throw std::runtime_error(
            "getNumCells failed with H3 error "
            + std::to_string(countError)
        );
    }

    if (globalStatistics.cellsChecked
        != static_cast<std::uint64_t>(expectedCellCount)) {

        throw std::runtime_error(
            "Unexpected number of evaluated cells: expected "
            + std::to_string(expectedCellCount)
            + ", evaluated "
            + std::to_string(globalStatistics.cellsChecked)
        );
    }

    return globalStatistics;
}

int parseResolution(const std::string& argument) {
    std::size_t parsedCharacters = 0;

    const int resolution = std::stoi(
        argument,
        &parsedCharacters
    );

    if (parsedCharacters != argument.size()) {
        throw std::invalid_argument(
            "Invalid resolution: " + argument
        );
    }

    if (resolution < 0 || resolution > 15) {
        throw std::invalid_argument(
            "Resolution must be in [0, 15]: " + argument
        );
    }

    return resolution;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr
                << "Usage: "
                << argv[0]
                << " <resolution> [resolution ...]\n"
                << "Example: "
                << argv[0]
                << " 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15\n";

            return 1;
        }

        std::cout
            << "resolution,"
            << "cells_checked,"
            << "max_center_to_vertex_m,"
            << "max_vertex_cell,"
            << "max_vertex_cell_is_pentagon,"
            << "max_center_to_boundary_m,"
            << "rounded_upper_bound_m,"
            << "max_boundary_cell,"
            << "max_boundary_cell_is_pentagon,"
            << "boundary_vertices\n";

        for (int argumentIndex = 1;
             argumentIndex < argc;
             ++argumentIndex) {

            const int resolution =
                    parseResolution(argv[argumentIndex]);

            requireOpenMpForResolution(resolution);

            int64_t expectedCellCount = 0;

            const H3Error error =
                    getNumCells(
                        resolution,
                        &expectedCellCount
                    );

            if (error != E_SUCCESS) {
                throw std::runtime_error(
                    "getNumCells failed with H3 error "
                    + std::to_string(error)
                );
            }

            std::cerr
                << "Evaluating resolution "
                << resolution
                << " ("
                << expectedCellCount
                << " cells, "
                << requestedThreadCount(resolution)
                << " thread(s))...\n";

            const Statistics statistics =
                    evaluateResolution(resolution);

            const Maximum& vertexMaximum =
                    statistics.maximumVertexDistance;
            const Maximum& boundaryMaximum =
                    statistics.maximumBoundaryDistance;

            const auto roundedUpperBound =
                    static_cast<std::uint64_t>(
                        std::ceil(
                            boundaryMaximum.distanceM
                        )
                    );

            std::cout
                << resolution << ','
                << statistics.cellsChecked << ','
                << std::fixed << std::setprecision(6)
                << static_cast<double>(
                    vertexMaximum.distanceM
                ) << ','
                << cellToHexString(vertexMaximum.cell) << ','
                << isPentagon(vertexMaximum.cell) << ','
                << static_cast<double>(
                    boundaryMaximum.distanceM
                ) << ','
                << roundedUpperBound << ','
                << cellToHexString(boundaryMaximum.cell) << ','
                << isPentagon(boundaryMaximum.cell) << ','
                << boundaryMaximum.numBoundaryVertices
                << '\n';
        }

        return 0;
    } catch (const std::exception& exception) {
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';

        return 1;
    }
}
