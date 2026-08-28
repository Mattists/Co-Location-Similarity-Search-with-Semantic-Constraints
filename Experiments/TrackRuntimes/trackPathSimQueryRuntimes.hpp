#ifndef TRACK_PATHSIM_QUERY_RUNTIMES_HPP
#define TRACK_PATHSIM_QUERY_RUNTIMES_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Sparse>

#include "DataHandler/MatrixIO.hpp"
#include "PathSim/PathSimQuery.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathEdgeDecomposition.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathGrid.hpp"

namespace track_runtimes {

namespace fs = std::filesystem;

constexpr char delimiter = ',';
constexpr int defaultK = 10;
constexpr int defaultMeasuredRuns = 10;
constexpr int warmupRuns = 1;
constexpr int noGridResolution = 0;
constexpr std::size_t sampleSize = 1000;
constexpr unsigned int querySampleSeed = 42;

struct TrackingConfig {
    std::string approach;
    bool weighted;
    int epsilon;
    int gridResolution;
    std::string datasetName;
    std::string matrixName;
    std::string spatialAttributeName;
    std::string attributeFileName;

    bool usesGrid() const {
        return gridResolution != noGridResolution;
    }

    bool usesEdgeDecomposition() const {
        return approach == "edgeDecomposition";
    }
};

template <typename T>
using QueryMatrix = Eigen::SparseMatrix<T, Eigen::RowMajor>;

inline std::string matrixArtifactName(const TrackingConfig& config) {
    if (config.usesGrid()) {
        return config.matrixName + "#" + config.spatialAttributeName + "_GridRangeQuery" + weightedName(config.weighted) +
               std::to_string(config.epsilon) + "_" + std::to_string(config.gridResolution);
    }

    if (config.usesEdgeDecomposition()) {
        return config.matrixName + "#" + config.spatialAttributeName + "_EdgeDecompositionRangeQuery" + weightedName(config.weighted) +
               std::to_string(config.epsilon);
    }
    throw std::invalid_argument("Unsupported tracking approach: " + config.approach);
}

inline void printUsage(const char* executableName) {
    std::cerr << "Usage: " << executableName
              << " <Datasets path> [k] [measuredRuns]" << std::endl;
}

inline double stddev(const std::vector<long long>& values,
                     const double mean) {
    if (values.size() < 2) {
        return 0.0;
    }

    double sqSum = 0.0;
    for (const long long value : values) {
        const double diff = static_cast<double>(value) - mean;
        sqSum += diff * diff;
    }
    return std::sqrt(sqSum / static_cast<double>(values.size() - 1));
}

template <typename T>
inline T rowValueSum(const QueryMatrix<T>& matrix,
                     const int row) {
    T sum = 0;
    for (typename QueryMatrix<T>::InnerIterator it(matrix, row); it; ++it) {
        sum += it.value();
    }
    return sum;
}

template <typename T>
inline QueryMatrix<T> loadTrackedMatrix(const fs::path& allDatasetPath,
                                        const TrackingConfig& config) {
    if (config.usesGrid()) {
        return loadGridCommutingMatrix<T>(allDatasetPath.string(),
                                          config.datasetName,
                                          config.matrixName,
                                          config.spatialAttributeName,
                                          config.attributeFileName,
                                          config.epsilon,
                                          config.weighted,
                                          delimiter,
                                          true,
                                          true,
                                          config.gridResolution);
    }

    if (config.usesEdgeDecomposition()) {
        CreateRTreeIfMissing(allDatasetPath.string(), config.datasetName, config.attributeFileName, delimiter);
        return loadEdgeDecompositionCommutingMatrix<T>(allDatasetPath.string(),
                                           config.datasetName,
                                           config.matrixName,
                                           config.spatialAttributeName,
                                           config.attributeFileName,
                                           config.epsilon,
                                           config.weighted,
                                           delimiter,
                                           true,
                                           true);
    }

    throw std::invalid_argument("Unsupported tracking approach: " + config.approach);
}  

inline void writeHeader(std::ostream& output) {
    output << "approach;weighted;epsilon;grid_resolution;conference;k;row_nonzeros;row_value_sum;"
           << "result_size;mean_ns;mean_us;mean_ms;stddev_ns;stddev_us;stddev_ms" << std::endl;
}

template <typename T>
inline void writeResultRow(std::ostream& output,
                           const TrackingConfig& config,
                           const int conference,
                           const int k,
                           const QueryMatrix<T>& matrix,
                           const std::size_t resultSize,
                           const double meanNs,
                           const double stddevNs) {
    output << config.approach << ";"
           << (config.weighted ? "true" : "false") << ";"
           << config.epsilon << ";"
           << config.gridResolution << ";"
           << conference << ";"
           << k << ";"
           << matrix.row(conference).nonZeros() << ";"
           << rowValueSum(matrix, conference) << ";"
           << resultSize << ";"
           << meanNs << ";"
           << meanNs / 1000.0 << ";"
           << meanNs / 1000000.0 << ";"
           << stddevNs << ";"
           << stddevNs / 1000.0 << ";"
           << stddevNs / 1000000.0 << std::endl;
}

template <typename T>
inline int runPathSimQueryRuntimeTrackingWithValueType(const fs::path& allDatasetPath,
                                                       const int k,
                                                       const int measuredRuns,
                                                       const TrackingConfig& config) {
    QueryMatrix<T> matrix = loadTrackedMatrix<T>(allDatasetPath, config);
    std::vector<T> deltas = loadDeltas<T>(matrixArtifactName(config),
                                          config.datasetName,
                                          allDatasetPath.string());

    if (static_cast<int>(deltas.size()) < matrix.rows()) {
        throw std::runtime_error("Delta count is smaller than the number of matrix rows for " +
                                 matrixArtifactName(config));
    }

    std::cout << std::fixed << std::setprecision(6);
    writeHeader(std::cout);

    std::vector<int> queryNodes(static_cast<std::size_t>(matrix.rows()));
    std::iota(queryNodes.begin(), queryNodes.end(), 0);
    if (queryNodes.size() > sampleSize) {
        std::mt19937 randomGenerator(querySampleSeed);
        std::shuffle(queryNodes.begin(), queryNodes.end(), randomGenerator);
        queryNodes.resize(sampleSize);
        std::sort(queryNodes.begin(), queryNodes.end());
    }

    for (const int conference : queryNodes) {
        Eigen::SparseVector<T, Eigen::RowMajor> queryVector(matrix.row(conference));
        std::vector<long long> runtimesNs;
        runtimesNs.reserve(measuredRuns);

        std::size_t lastResultSize = 0;

        for (int run = 0; run < warmupRuns + measuredRuns; run++) {
            const bool isWarmup = run < warmupRuns;

            const auto t1 = std::chrono::steady_clock::now();
            std::set<std::pair<double, int>, std::greater<>> results =
                    PathSimTopKQuery(matrix,
                                     deltas,
                                     queryVector,
                                     deltas[conference],
                                     k);
            const auto t2 = std::chrono::steady_clock::now();

            volatile std::size_t preventOptimization = results.size();

            if (!isWarmup) {
                runtimesNs.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
                lastResultSize = preventOptimization;
            }
        }

        const double meanNs = std::accumulate(runtimesNs.begin(), runtimesNs.end(), 0.0) /
                              static_cast<double>(runtimesNs.size());
        const double stddevNs = stddev(runtimesNs, meanNs);

        writeResultRow(std::cout,
                       config,
                       conference,
                       k,
                       matrix,
                       lastResultSize,
                       meanNs,
                       stddevNs);
    }

    return 0;
}

inline int runPathSimQueryRuntimeTracking(int argc,
                                          char* argv[],
                                          const TrackingConfig& config) {
    if (argc < 2 || argc > 4) {
        printUsage(argv[0]);
        return 1;
    }

    const fs::path allDatasetPath = argv[1];
    const int k = argc > 2 ? std::stoi(argv[2]) : defaultK;
    const int measuredRuns = argc > 3 ? std::stoi(argv[3]) : defaultMeasuredRuns;

    if (k < 1) {
        std::cerr << "k must be at least 1." << std::endl;
        return 1;
    }
    if (measuredRuns < 1) {
        std::cerr << "measuredRuns must be at least 1." << std::endl;
        return 1;
    }

    if (config.weighted) {
        return runPathSimQueryRuntimeTrackingWithValueType<long double>(allDatasetPath,
                                                                        k,
                                                                        measuredRuns,
                                                                        config);
    }
    return runPathSimQueryRuntimeTrackingWithValueType<unsigned long long>(allDatasetPath,
                                                                          k,
                                                                          measuredRuns,
                                                                          config);
}

} // namespace track_runtimes

#endif // TRACK_PATHSIM_QUERY_RUNTIMES_HPP
