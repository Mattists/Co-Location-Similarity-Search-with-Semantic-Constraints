#ifndef APPROXIMATION_QUALITY_HPP
#define APPROXIMATION_QUALITY_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Eigen/Sparse>

#include "DataHandler/MatrixIO.hpp"
#include "DataHandler/Preprocessor.hpp"
#include "PathSim/PathSimQuery.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathEdgeDecomposition.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathGrid.hpp"

namespace approximation_quality {

namespace fs = std::filesystem;

constexpr char delimiter = ',';
constexpr int referenceEpsilon = 1000;
constexpr bool useWeightedEdges = false;
constexpr std::size_t yfccQuerySampleSize = 10000;
constexpr unsigned int querySampleSeed = 42;
constexpr std::array<std::size_t, 6> topKValues {1, 5, 10, 25, 50, 100};

// Represents the configuration for a dataset, including its name, matrix name, spatial node name, and attribute file name.
struct DatasetConfig {
    std::string name;
    std::string matrixName;
    std::string spatialNodeName;
    std::string attributeFileName;
};

// Represents the configuration for an approach, including its label, whether it uses a grid, the epsilon value, and the grid resolution.
struct ApproachConfig {
    std::string label;
    bool usesGrid;
    int epsilon;
    int gridResolution;
};

using MatrixValue = unsigned long long;
using RowMajorMatrix = Eigen::SparseMatrix<MatrixValue, Eigen::RowMajor>;

// Represents the context of a matrix, including its label, the commuting matrix, and the deltas associated with it.
struct MatrixContext {
    std::string label;
    RowMajorMatrix matrix;
    std::vector<MatrixValue> deltas;
};

// Represents a score entry for a node, including the node identifier and its associated score.
struct ScoreEntry {
    int node;
    long double score;
};

// Represents the PathSim scores for a query object, including the sorted scores and a mapping of nodes to their scores.
struct QueryScores {
    std::vector<ScoreEntry> sortedByScore;
    std::unordered_map<int, long double> byNode;
};

// Represents the top-k ranking metrics for a query object for a given k, including the count of top overlaps, eligibility for top overlap, nDCG score, and eligibility for nDCG.
struct RankingMetrics {
    std::size_t topOverlapCount = 0;
    bool topOverlapEligible = false;
    long double ndcg = 0.0L;
    bool ndcgEligible = false;
};

// Represents the metrics for a query object, including precision, recall, error metrics, result counts, and ranking metrics.
struct QueryMetrics {
    long double precision = 0.0L;
    long double recall = 0.0L;
    long double mae = 0.0L;
    long double mse = 0.0L;
    std::size_t referenceResultCount = 0;
    std::size_t candidateResultCount = 0;
    bool precisionEligible = false;
    bool recallEligible = false;
    bool maeEligible = false;
    std::array<RankingMetrics, topKValues.size()> rankings; // Stores the ranking metrics for each top-k value.
};

// Represents the aggregated metrics across multiple query objects, including counts and sums for quality metrics.
struct RunningMetrics {
    std::size_t queryCount = 0;
    std::size_t precisionEligibleQueryCount = 0;
    std::size_t recallEligibleQueryCount = 0;
    std::size_t precisionEmptyQueryCount = 0;
    std::size_t recallEmptyQueryCount = 0;
    std::size_t maeEligibleQueryCount = 0;
    std::size_t maeEmptyUnionQueryCount = 0;
    std::array<std::size_t, topKValues.size()> topOverlapEligibleQueryCounts {};
    std::array<std::size_t, topKValues.size()> topOverlapIneligibleQueryCounts {};
    std::array<std::size_t, topKValues.size()> ndcgEligibleQueryCounts {};
    std::array<std::size_t, topKValues.size()> ndcgIneligibleQueryCounts {};
    long double precisionSum = 0.0L;
    long double recallSum = 0.0L;
    long double maeSum = 0.0L;
    long double mseSum = 0.0L;
    long double referenceResultCountSum = 0.0L;
    long double candidateResultCountSum = 0.0L;
    std::array<long double, topKValues.size()> topOverlapCountSums {};
    std::array<long double, topKValues.size()> ndcgSums {};

    void add(const QueryMetrics& metrics) {
        queryCount++;
        referenceResultCountSum += static_cast<long double>(metrics.referenceResultCount);
        candidateResultCountSum += static_cast<long double>(metrics.candidateResultCount);

        if (metrics.precisionEligible) {
            precisionEligibleQueryCount++;
            precisionSum += metrics.precision;
        } else {
            precisionEmptyQueryCount++;
        }

        if (metrics.recallEligible) {
            recallEligibleQueryCount++;
            recallSum += metrics.recall;
        }else {
            recallEmptyQueryCount++;
        }

        if (metrics.maeEligible) {
            maeEligibleQueryCount++;
            maeSum += metrics.mae;
            mseSum += metrics.mse;
        } else {
            maeEmptyUnionQueryCount++;
        }

        for (std::size_t i = 0; i < topKValues.size(); i++) {
            const RankingMetrics& ranking = metrics.rankings[i];
            if (ranking.topOverlapEligible) {
                topOverlapEligibleQueryCounts[i]++;
                topOverlapCountSums[i] += static_cast<long double>(ranking.topOverlapCount);
            } else {
                topOverlapIneligibleQueryCounts[i]++;
            }

            if (ranking.ndcgEligible) {
                ndcgEligibleQueryCounts[i]++;
                ndcgSums[i] += ranking.ndcg;
            } else {
                ndcgIneligibleQueryCounts[i]++;
            }
        }
    }

    long double average(const long double sum,
                        const std::size_t count) const {
        if (count == 0) {
            return 0.0L;
        }
        return sum / static_cast<long double>(count);
    }
};

// Returns a string representation of the name of the matrix artifact.
inline std::string matrixArtifactName(const DatasetConfig& dataset,
                                      const ApproachConfig& approach) {
    if (approach.usesGrid) {
        return dataset.matrixName + "#" + dataset.spatialNodeName + "_GridRangeQuery" +
               weightedName(useWeightedEdges) + std::to_string(approach.epsilon) +
               "_" + std::to_string(approach.gridResolution);
    }

    return dataset.matrixName + "#" + dataset.spatialNodeName + "_EdgeDecompositionRangeQuery" +
           weightedName(useWeightedEdges) + std::to_string(approach.epsilon);
}

// Loads the commuting matrix and deltas for the given dataset and approach.
inline MatrixContext loadMatrixContext(const fs::path& datasetsPath,
                                       const DatasetConfig& dataset,
                                       const ApproachConfig& approach) {
    MatrixContext context;
    context.label = approach.label;

    if (approach.usesGrid) {
        context.matrix = loadGridCommutingMatrix<MatrixValue>(datasetsPath.string(),
                                                              dataset.name,
                                                              dataset.matrixName,
                                                              dataset.spatialNodeName,
                                                              dataset.attributeFileName,
                                                              approach.epsilon,
                                                              useWeightedEdges,
                                                              delimiter,
                                                              true,
                                                              true,
                                                              approach.gridResolution);
    } else {
        CreateRTreeIfMissing(datasetsPath.string(), dataset.name, dataset.attributeFileName, delimiter);
        context.matrix = loadEdgeDecompositionCommutingMatrix<MatrixValue>(datasetsPath.string(),
                                                                           dataset.name,
                                                                           dataset.matrixName,
                                                                           dataset.spatialNodeName,
                                                                           dataset.attributeFileName,
                                                                           approach.epsilon,
                                                                           useWeightedEdges,
                                                                           delimiter,
                                                                           true,
                                                                           true);
    }

    context.matrix.makeCompressed();
    context.deltas = loadDeltas<MatrixValue>(matrixArtifactName(dataset, approach),
                                             dataset.name,
                                             datasetsPath.string());

    if (static_cast<int>(context.deltas.size()) < context.matrix.rows()) {
        throw std::runtime_error("Delta count is smaller than the number of matrix rows for " +
                                 matrixArtifactName(dataset, approach) + ".");
    }

    return context;
}

// Computes the PathSim scores for all nodes in the matrix with respect to the given query node.
inline QueryScores computeQueryScores(const MatrixContext& context,
                                      const int queryNode) {
    Eigen::SparseVector<MatrixValue, Eigen::RowMajor> queryVector(context.matrix.row(queryNode));
    std::set<std::pair<double, int>, std::greater<>> pathSimResults =
            PathSimTopKQuery(context.matrix,
                             context.deltas,
                             queryVector,
                             context.deltas[static_cast<std::size_t>(queryNode)],
                             context.matrix.rows());

    QueryScores scores;
    scores.sortedByScore.reserve(pathSimResults.size());
    scores.byNode.reserve(pathSimResults.size());

    for (const auto& [score, node] : pathSimResults) {
        if (score <= 0.0 || !std::isfinite(score)) {
            continue;
        }

        scores.sortedByScore.push_back({node, static_cast<long double>(score)});
        scores.byNode.emplace(node, static_cast<long double>(score));
    }

    return scores;
}

// Creates a vector of query nodes based on the specified query count, sample size, and random seed. If the sample size is zero or greater than the query count, all query nodes are returned. Otherwise, a random sample of query nodes is selected and sorted by ID (== row number in commuting matrix).
inline std::vector<int> createQueryNodes(const int queryCount,
                                         const std::size_t sampleSize,
                                         const unsigned int seed) {
    std::vector<int> queryNodes;
    queryNodes.reserve(static_cast<std::size_t>(queryCount));
    for (int queryNode = 0; queryNode < queryCount; queryNode++) {
        queryNodes.push_back(queryNode);
    }

    if (sampleSize == 0 || queryNodes.size() <= sampleSize) {
        return queryNodes;
    }

    std::mt19937 randomGenerator(seed);
    std::shuffle(queryNodes.begin(), queryNodes.end(), randomGenerator);
    queryNodes.resize(sampleSize);
    std::sort(queryNodes.begin(), queryNodes.end());
    return queryNodes;
}

// Computes the top K nodes from the given scores, excluding the query node itself.
inline std::vector<int> topKNodesWithoutQueryNode(const QueryScores& scores,
                                                  const int queryNode,
                                                  const std::size_t topK) {
    std::vector<int> topNodes;
    topNodes.reserve(topK);

    for (const ScoreEntry& entry : scores.sortedByScore) {
        if (entry.node == queryNode) {
            continue;
        }
        topNodes.push_back(entry.node);
        if (topNodes.size() == topK) {
            break;
        }
    }

    return topNodes;
}

// Computes the discounted gain for a given relevance and rank index. Needed to compute nDCG. The rank index is zero-based, so the first rank has an index of 0.
inline long double discountedGain(const long double relevance,
                                  const std::size_t rankIndex) {
    return (std::pow(2.0L, relevance) - 1.0L) /
           std::log2(static_cast<long double>(rankIndex) + 2.0L);
}

// Uses reference scores as graded relevance for nDCG and keeps top-overlap eligibility unchanged.
inline RankingMetrics computeRankingMetrics(const QueryScores& reference,
                                            const QueryScores& candidate,
                                            const int queryNode,
                                            const std::size_t topK) {
    RankingMetrics metrics;
    const std::vector<int> referenceTop = topKNodesWithoutQueryNode(reference, queryNode, topK);
    const std::vector<int> candidateTop = topKNodesWithoutQueryNode(candidate, queryNode, topK);

    if (referenceTop.size() >= topK) {
        std::unordered_set<int> referenceNodes(referenceTop.begin(), referenceTop.end());
        for (const int node : candidateTop) {
            if (referenceNodes.contains(node)) {
                metrics.topOverlapCount++;
            }
        }
        metrics.topOverlapEligible = true;
    }

    long double dcg = 0.0L;
    for (std::size_t rank = 0; rank < candidateTop.size(); rank++) {
        const auto referenceIt = reference.byNode.find(candidateTop[rank]);
        if (referenceIt != reference.byNode.end()) {
            dcg += discountedGain(referenceIt->second, rank);
        }
    }

    long double idealDcg = 0.0L;
    std::size_t idealRank = 0;
    for (const ScoreEntry& entry : reference.sortedByScore) {
        if (entry.node == queryNode) {
            continue;
        }

        idealDcg += discountedGain(entry.score, idealRank);
        idealRank++;
        if (idealRank == topK) {
            break;
        }
    }

    if (referenceTop.size() >= topK && idealDcg > 0.0L) {
        metrics.ndcg = dcg / idealDcg;
        metrics.ndcgEligible = true;
    }

    return metrics;
}

// Compares the query scores of the reference and candidate approaches for a given query node.
inline QueryMetrics compareQueryScores(const QueryScores& reference,
                                       const QueryScores& candidate,
                                       const int queryNode) {
    QueryMetrics metrics;

    std::size_t intersectionCount = 0;
    for (const auto& [node, candidateScore] : candidate.byNode) {
        if (node == queryNode) {
            continue;
        }

        metrics.candidateResultCount++;
        if (reference.byNode.contains(node)) {
            intersectionCount++;
        }
    }

    for (const auto& [node, referenceScore] : reference.byNode) {
        if (node != queryNode) {
            metrics.referenceResultCount++;
        }
    }

    metrics.precisionEligible = metrics.candidateResultCount > 0;
    if (metrics.precisionEligible) {
        metrics.precision = static_cast<long double>(intersectionCount) / static_cast<long double>(metrics.candidateResultCount);
    }

    metrics.recallEligible = metrics.referenceResultCount > 0;
    if (metrics.recallEligible) {
        metrics.recall = static_cast<long double>(intersectionCount) / static_cast<long double>(metrics.referenceResultCount);
    }

    long double absoluteErrorSum = 0.0L;
    long double squaredErrorSum = 0.0L;
    std::size_t unionCount = 0;

    for (const auto& [node, referenceScore] : reference.byNode) {
        if (node == queryNode) {
            continue;
        }

        const auto candidateIt = candidate.byNode.find(node);
        const long double candidateScore = candidateIt == candidate.byNode.end() ? 0.0L : candidateIt->second;
        const long double error = referenceScore - candidateScore;
        absoluteErrorSum += std::abs(error);
        squaredErrorSum += error * error;
        unionCount++;
    }

    for (const auto& [node, candidateScore] : candidate.byNode) {
        if (node == queryNode || reference.byNode.contains(node)) {
            continue;
        }

        absoluteErrorSum += std::abs(candidateScore);
        squaredErrorSum += candidateScore * candidateScore;
        unionCount++;
    }

    metrics.maeEligible = unionCount > 0;
    if (metrics.maeEligible) {
        metrics.mae = absoluteErrorSum / static_cast<long double>(unionCount);
        metrics.mse = squaredErrorSum / static_cast<long double>(unionCount);
    }
    for (std::size_t i = 0; i < topKValues.size(); i++) {
        metrics.rankings[i] = computeRankingMetrics(reference,
                                                    candidate,
                                                    queryNode,
                                                    topKValues[i]);
    }

    return metrics;
}

// Compares PathSim results of all query objects between the reference and candidate matrices, aggregating the metrics across all queries.
inline RunningMetrics compareApproaches(const MatrixContext& reference,
                                        const MatrixContext& candidate,
                                        const std::vector<int>& queryNodes) {
    if (reference.matrix.rows() != candidate.matrix.rows()) {
        throw std::runtime_error("Matrix row counts differ between " + reference.label +
                                 " and " + candidate.label + ".");
    }

    RunningMetrics metrics;

    for (const int queryNode : queryNodes) {
        QueryScores referenceQueryScores = computeQueryScores(reference, queryNode);
        QueryScores candidateQueryScores = computeQueryScores(candidate, queryNode);
        metrics.add(compareQueryScores(referenceQueryScores,
                                       candidateQueryScores,
                                       queryNode));
    }

    return metrics;
}

// Writes the header row for the output metrics.
inline void writeHeader(std::ostream& output) {
    output << "dataset;reference_epsilon;candidate_epsilon;candidate_grid_resolution;total_queries;"
           << "avg_reference_results_all_queries;avg_candidate_results_all_queries;"
           << "precision_empty_queries;avg_precision;"
           << "recall_empty_queries;avg_recall;"
           << "mae_empty_union_queries;avg_mae;"
           << "mse_empty_union_queries;avg_mse";

    for (const std::size_t topK : topKValues) {
        output << ";top" << topK << "_ineligible_queries"
               << ";avg_top" << topK << "_overlap";
    }

    for (const std::size_t topK : topKValues) {
        output << ";ndcg_at" << topK << "_ineligible_queries"
               << ";avg_ndcg_at" << topK;
    }

    output << std::endl;
}

// Writes a row of metrics for the given dataset, reference and candidate approaches, and running metrics to the output stream.
inline void writeMetricsRow(std::ostream& output,
                            const DatasetConfig& dataset,
                            const ApproachConfig& reference,
                            const ApproachConfig& candidate,
                            const RunningMetrics& metrics) {
    output << dataset.name << ';'
           << reference.epsilon << ';'
           << candidate.epsilon << ';'
           << (candidate.usesGrid ? candidate.gridResolution : 0) << ';'
           << metrics.queryCount << ';'
           << metrics.average(metrics.referenceResultCountSum, metrics.queryCount) << ';'
           << metrics.average(metrics.candidateResultCountSum, metrics.queryCount) << ';'
           << metrics.precisionEmptyQueryCount << ';'
           << metrics.average(metrics.precisionSum, metrics.precisionEligibleQueryCount) << ';'
           << metrics.recallEmptyQueryCount << ';'
           << metrics.average(metrics.recallSum, metrics.recallEligibleQueryCount) << ';'
           << metrics.maeEmptyUnionQueryCount << ';'
           << metrics.average(metrics.maeSum, metrics.maeEligibleQueryCount) << ';'
           << metrics.maeEmptyUnionQueryCount << ';'
           << metrics.average(metrics.mseSum, metrics.maeEligibleQueryCount);

    for (std::size_t i = 0; i < topKValues.size(); i++) {
        output << ';' << metrics.topOverlapIneligibleQueryCounts[i]
               << ';' << metrics.average(metrics.topOverlapCountSums[i], metrics.topOverlapEligibleQueryCounts[i]) / static_cast<long double>(topKValues[i]);
    }

    for (std::size_t i = 0; i < topKValues.size(); i++) {
        output << ';' << metrics.ndcgIneligibleQueryCounts[i]
               << ';' << metrics.average(metrics.ndcgSums[i], metrics.ndcgEligibleQueryCounts[i]);
    }

    output << std::endl;
}

// Writes all metrics rows for the given dataset and comparison results to the output stream.
inline void writeResults(std::ostream& output,
                         const DatasetConfig& dataset,
                         const ApproachConfig& reference,
                         const std::vector<std::pair<ApproachConfig, RunningMetrics>>& results) {
    output << std::fixed << std::setprecision(8);
    writeHeader(output);
    for (const auto& [candidate, metrics] : results) {
        writeMetricsRow(output, dataset, reference, candidate, metrics);
    }
}

inline std::string lowerCaseDatasetName(std::string datasetName) {
    std::transform(datasetName.begin(),
                   datasetName.end(),
                   datasetName.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return datasetName;
}

inline fs::path defaultSummaryPath(const DatasetConfig& dataset) {
    return fs::path("ApproximationQuality") /
           ("approximation_quality_" + lowerCaseDatasetName(dataset.name) + ".csv");
}

// Runs the approximation quality comparison for the given dataset, loading the reference and candidate commuting matrices, computing the metrics, and writing the results to the output stream.
inline int runApproximationQuality(int argc,
                                   char* argv[],
                                   const DatasetConfig& dataset,
                                   const int gridResolution,
                                   const int maxGridDist) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <Datasets path> [summary csv path]" << std::endl;
        return 1;
    }

    const fs::path datasetsPath = argv[1];
    const int baseEpsilon = referenceEpsilon / 2;
    const int maxGridEpsilon = baseEpsilon + maxGridDist;
    const ApproachConfig reference {
            "edge_decomposition_eps1000_unweighted",
            false,
            referenceEpsilon,
            0
    };
    const std::vector<ApproachConfig> candidates {
            {"grid_r" + std::to_string(gridResolution) + "_eps" + std::to_string(baseEpsilon) + "_unweighted",
             true,
             baseEpsilon,
             gridResolution},
            {"grid_r" + std::to_string(gridResolution) + "_eps" + std::to_string(maxGridEpsilon) + "_unweighted",
             true,
             maxGridEpsilon,
             gridResolution}
    };

    preprocessDataset<MatrixValue>(dataset.name, datasetsPath.string(), true);

    MatrixContext referenceMatrix = loadMatrixContext(datasetsPath, dataset, reference);
    // Sample before any eligibility checks so empty or ineligible queries remain part of the metrics.
    const std::vector<int> queryNodes = createQueryNodes(referenceMatrix.matrix.rows(),
                                                         dataset.name == "YFCC" ? yfccQuerySampleSize : 0,
                                                         querySampleSeed);
    std::vector<std::pair<ApproachConfig, RunningMetrics>> results;
    results.reserve(candidates.size());

    for (const ApproachConfig& candidate : candidates) {
        MatrixContext candidateMatrix = loadMatrixContext(datasetsPath, dataset, candidate);
        const RunningMetrics metrics = compareApproaches(referenceMatrix,
                                                         candidateMatrix,
                                                         queryNodes);
        results.emplace_back(candidate, metrics);
    }

    const fs::path summaryPath = argc == 3 ? fs::path(argv[2]) : defaultSummaryPath(dataset);
    if (!summaryPath.parent_path().empty()) {
        fs::create_directories(summaryPath.parent_path());
    }

    std::ofstream summaryFile(summaryPath);
    if (!summaryFile.is_open()) {
        throw std::runtime_error("Could not open summary CSV for writing: " + summaryPath.string());
    }
    writeResults(summaryFile, dataset, reference, results);

    return 0;
}

} // namespace approximation_quality

#endif // APPROXIMATION_QUALITY_HPP
