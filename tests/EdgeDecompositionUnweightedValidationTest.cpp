#include <filesystem>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <Eigen/Sparse>
#include "gtest/gtest.h"

#include "DataHandler/MatrixIO.hpp"
#include "DataHandler/Preprocessor.hpp"
#include "PathSim/PathSimQuery.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathHelper.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathEdgeDecomposition.hpp"

namespace fs = std::filesystem;
using namespace Eigen;
using namespace std;

#ifndef SPATIALMETAPATH_DATASETS_DIR
#define SPATIALMETAPATH_DATASETS_DIR "Datasets"
#endif

namespace {

const string kDatasetName = "EdgeDecompositionValidation";
const string kSpatialAttributesFile = "placesWGS84.csv";
constexpr double kLongitudeStepDegrees = 0.00899320363724538;

using TopKResult = set<pair<double, int>, greater<>>;

fs::path datasetRootPath() {
    return fs::path(SPATIALMETAPATH_DATASETS_DIR);
}

fs::path sourceDatasetPath() {
    return datasetRootPath() / kDatasetName;
}

fs::path matrixDirPath() {
    return sourceDatasetPath() / "matrices";
}

int epsilonMetersForKilometers(const int kilometers) {
    if (kilometers == 0) {
        return 0;
    }
    // Include exact kilometer neighbors even if Haversine rounding lands just above the boundary.
    return kilometers * 1000 + 1;
}

void removeIfExists(const fs::path& filePath) {
    std::error_code error;
    if (fs::exists(filePath) && !fs::remove(filePath, error)) {
        throw std::runtime_error("Could not remove file " + filePath.string() + ": " + error.message());
    }
}

void removeEdgeDecompositionArtifacts(const int epsilonMeters) {
    const string baseName = "IP#place_EdgeDecompositionRangeQueryUnweighted" + std::to_string(epsilonMeters);
    removeIfExists(matrixDirPath() / (baseName + ".mtx"));
    removeIfExists(matrixDirPath() / (baseName + ".mtx.bin"));
    removeIfExists(matrixDirPath() / (baseName + ".delta"));
    removeIfExists(matrixDirPath() / (baseName + ".delta.bin"));
}

TopKResult topKForQueryNode(const SparseMatrix<unsigned long long, RowMajor>& matrix,
                            const std::vector<unsigned long long>& deltas,
                            const int queryNode) {
    const SparseVector<unsigned long long, RowMajor> queryVector(matrix.row(queryNode));
    return PathSimTopKQuery(matrix,
                            deltas,
                            queryVector,
                            deltas.at(queryNode),
                            matrix.rows());
}

} // namespace

TEST(EdgeDecompositionUnweightedValidationTest, areResultsCorrectForZeroToFourKilometers) {
    for (int i = 0; i < 4; ++i) {
        const Coordinate left{kLongitudeStepDegrees * i, 0.0};
        const Coordinate right{kLongitudeStepDegrees * (i + 1), 0.0};
        EXPECT_NEAR(1000.0, haversineDistanceMeters(left, right), 1e-8);
    }

    ASSERT_TRUE(fs::exists(sourceDatasetPath()));
    removeIfExists(sourceDatasetPath() / "placesWGS84_RTree.dat");

    preprocessDataset<unsigned long long>(kDatasetName,
                                          datasetRootPath().string(),
                                          false,
                                          false,
                                          false);
    CreateRTreeIfMissing(datasetRootPath().string(),
                         kDatasetName,
                         kSpatialAttributesFile,
                         ',');

    SparseMatrix<unsigned long long, RowMajor> itemPlaceMatrix;
    ASSERT_TRUE(loadMatrix<unsigned long long>(
            matrixDirPath() / "IP.mtx",
            itemPlaceMatrix));
    ASSERT_EQ(3, itemPlaceMatrix.rows());
    ASSERT_EQ(5, itemPlaceMatrix.cols());

    const std::vector<std::pair<int, std::vector<TopKResult>>> expectations{
            {0, {
                    {{1.0, 0}},
                    {{1.0, 1}},
                    {{1.0, 2}},
            }},
            {1, {
                    {{1.0, 0}, {2.0 / 10.0, 1}},
                    {{1.0, 1}, {2.0 / 10.0, 0}, {2.0 / 15.0, 2}},
                    {{1.0, 2}, {2.0 / 15.0, 1}},
            }},
            {2, {
                    {{1.0, 0}, {4.0 / 14.0, 1}},
                    {{1.0, 1}, {6.0 / 20.0, 2}, {4.0 / 14.0, 0}},
                    {{1.0, 2}, {6.0 / 20.0, 1}},
            }},
            {3, {
                    {{1.0, 0}, {4.0 / 16.0, 1}, {2.0 / 15.0, 2}},
                    {{1.0, 1}, {4.0 / 16.0, 0}, {8.0 / 23.0, 2}},
                    {{1.0, 2}, {8.0 / 23.0, 1}, {2.0 / 15.0, 0}},
            }},
            {4, {
                    {{1.0, 0}, {4.0 / 17.0, 1}, {4.0 / 17.0, 2}},
                    {{1.0, 1}, {8.0 / 24.0, 2}, {4.0 / 17.0, 0}},
                    {{1.0, 2}, {8.0 / 24.0, 1}, {4.0 / 17.0, 0}},
            }},
    };

    for (const auto& [kilometers, expectedByQueryNode] : expectations) {
        SCOPED_TRACE("Epsilon " + std::to_string(kilometers) + " km");
        const int epsilonMeters = epsilonMetersForKilometers(kilometers);
        removeEdgeDecompositionArtifacts(epsilonMeters);

        const auto edgeDecompositionMatrix = loadEdgeDecompositionCommutingMatrix<unsigned long long>(
                datasetRootPath().string(),
                kDatasetName,
                "IP",
                "place",
                kSpatialAttributesFile,
                epsilonMeters,
                false,
                ',',
                false,
                true);
        ASSERT_EQ(3, edgeDecompositionMatrix.rows());

        const std::vector<unsigned long long> deltas =
                loadDeltas<unsigned long long>("IP#place_EdgeDecompositionRangeQueryUnweighted" +
                                                       std::to_string(epsilonMeters),
                                               kDatasetName,
                                               datasetRootPath().string());

        ASSERT_EQ(expectedByQueryNode.size(), static_cast<std::size_t>(edgeDecompositionMatrix.rows()));
        for (int queryNode = 0; queryNode < edgeDecompositionMatrix.rows(); ++queryNode) {
            SCOPED_TRACE("Query node " + std::to_string(queryNode));
            ASSERT_EQ(expectedByQueryNode[queryNode],
                      topKForQueryNode(edgeDecompositionMatrix, deltas, queryNode));
        }
    }
}
