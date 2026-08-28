#ifndef FOURSQUARE_USER_PATHSIM_SCORES_HPP
#define FOURSQUARE_USER_PATHSIM_SCORES_HPP

#include <cstddef>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Sparse>

#include "DataHandler/MatrixIO.hpp"
#include "DataHandler/Preprocessor.hpp"
#include "PathSim/PathSimQuery.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathEdgeDecomposition.hpp"

using namespace std;
using namespace Eigen;

namespace fs = filesystem;

namespace foursquare_user_pathsim {

constexpr char datasetName[] = "Foursquare";
constexpr char matrixName[] = "checked_in_at";
constexpr char spatialNodeName[] = "venue";
constexpr char spatialAttributeFileName[] = "venuesWGS84.csv";
constexpr char delimiter = ',';
constexpr int epsilonMeters = 100;
constexpr int userNodeType = 0;

using RegularValue = unsigned long long;

// This struct holds the results of a PathSim score computation, including the dot product of the two user nodes' rows in the matrix, their respective delta values, and the final PathSim score.
struct PathSimResult {
    long double dotProduct = 0.0L;
    long double firstDelta = 0.0L;
    long double secondDelta = 0.0L;
    long double score = 0.0L;
};

inline string regularDeltaArtifactName() {
    return string(matrixName) + "#(" + matrixName + ")^-1";
}

inline string edgeDecompositionArtifactName(const bool weighted) {
    return string(matrixName) + "#" + spatialNodeName + "_EdgeDecompositionRangeQuery" + weightedName(weighted) + to_string(epsilonMeters);
}

// Ensures that the base matrix and its corresponding delta file exist for the Foursquare dataset.
inline void ensureFoursquareBaseMatrixExists(const fs::path& datasetsPath) {
    const fs::path matrixPath = datasetsPath / datasetName / "matrices" / (string(matrixName) + ".mtx");
    const fs::path binaryMatrixPath = fs::path(matrixPath.string() + ".bin");
    const fs::path deltaPath = datasetsPath / datasetName / "matrices" / (regularDeltaArtifactName() + ".delta");
    const fs::path binaryDeltaPath = fs::path(deltaPath.string() + ".bin");

    const bool matrixExists = fs::exists(matrixPath) || fs::exists(binaryMatrixPath);
    const bool deltaExists = fs::exists(deltaPath) || fs::exists(binaryDeltaPath);
    if (matrixExists && deltaExists) {
        return;
    }

    cout << "Base matrix or regular deltas for " << matrixName << " are missing. Preprocessing " << datasetName << " first." << endl;
    preprocessDataset<RegularValue>(datasetName, datasetsPath.string(), true);
}

// Loads the node type for a given node ID from the nodes.csv file in the specified dataset directory.
inline int loadNodeType(const fs::path& datasetsPath,
                        const int nodeId) {
    const fs::path nodesPath = datasetsPath / datasetName / "nodes.csv";
    ifstream nodesFile(nodesPath);
    if (!nodesFile.is_open()) {
        throw runtime_error("Could not open nodes file: " + nodesPath.string());
    }

    string line;
    for (int currentNodeId = 0; getline(nodesFile, line); currentNodeId++) {
        if (currentNodeId == nodeId) {
            return stoi(line);
        }
    }

    throw runtime_error("Node ID " + to_string(nodeId) + " is not present in " + nodesPath.string());
}

// Counts all nodes of a given type in the specified dataset directory.
inline int loadNodeTypeCount(const fs::path& datasetsPath,
                             const int requestedNodeType) {
    const fs::path nodesPath = datasetsPath / datasetName / "nodes.csv";
    ifstream nodesFile(nodesPath);
    if (!nodesFile.is_open()) {
        throw runtime_error("Could not open nodes file: " + nodesPath.string());
    }

    int count = 0;
    string line;
    while (getline(nodesFile, line)) {
        if (stoi(line) == requestedNodeType) {
            count++;
        }
    }

    return count;
}

// Validates that the specified node ID corresponds to a Foursquare user node.
inline void validateUserNode(const fs::path& datasetsPath,
                             const int nodeId) {
    const int nodeType = loadNodeType(datasetsPath, nodeId);
    if (nodeType != userNodeType) {
        throw runtime_error("Node ID " + to_string(nodeId) + " is not a Foursquare user node.");
    }
}

// Computes the dot product of two rows in a sparse matrix.
template <typename T>
long double rowDotProduct(const SparseMatrix<T, RowMajor>& matrix,
                          const int leftRow,
                          const int rightRow) {
    SparseVector<T, RowMajor> left(matrix.row(leftRow));
    SparseVector<T, RowMajor> right(matrix.row(rightRow));

    long double dotProduct = 0.0L;
    typename SparseVector<T, RowMajor>::InnerIterator leftIt(left);
    typename SparseVector<T, RowMajor>::InnerIterator rightIt(right);

    while (leftIt && rightIt) {
        if (leftIt.index() == rightIt.index()) {
            dotProduct += static_cast<long double>(leftIt.value()) * static_cast<long double>(rightIt.value());
            ++leftIt;
            ++rightIt;
        } else if (leftIt.index() < rightIt.index()) {
            ++leftIt;
        } else {
            ++rightIt;
        }
    }

    return dotProduct;
}

// Computes the PathSim score between two user nodes in a sparse matrix.
template <typename T>
PathSimResult computePathSimScore(const SparseMatrix<T, RowMajor>& matrix,
                                  const vector<T>& deltas,
                                  const int firstNodeId,
                                  const int secondNodeId) {
    PathSimResult result;
    result.dotProduct = rowDotProduct(matrix, firstNodeId, secondNodeId);
    result.firstDelta = static_cast<long double>(deltas[static_cast<size_t>(firstNodeId)]);
    result.secondDelta = static_cast<long double>(deltas[static_cast<size_t>(secondNodeId)]);

    const long double denominator = result.firstDelta + result.secondDelta;
    result.score = denominator == 0.0L ? 0.0L : (2.0L * result.dotProduct) / denominator;
    return result;
}

// Computes the position of a given node in the PathSi top-k query ranking.
template <typename T>
int pathSimTopKPosition(const SparseMatrix<T, RowMajor>& matrix,
                        const vector<T>& deltas,
                        const int queryNodeId,
                        const int targetNodeId,
                        const int k) {
    const set<pair<double, int>, greater<>> ranking = PathSimTopKQuery<T>(matrix,
                                                                          deltas,
                                                                          SparseVector<T, RowMajor>(matrix.row(queryNodeId)),
                                                                          deltas[static_cast<size_t>(queryNodeId)],
                                                                          k);
    int position = 1;
    for (const auto& result : ranking) {
        if (result.second == targetNodeId) {
            return position;
        }
        position++;
    }

    return -1;
}

// Appends the position of a target node in the ranking to the output stream.
inline void appendRankingPosition(ostringstream& output,
                                  const string& approachName,
                                  const int queryNodeId,
                                  const int targetNodeId,
                                  const int position) {
    output << approachName << "_position_of_" << targetNodeId << "_in_query_" << queryNodeId << "_ranking = ";
    if (position < 0) {
        output << "not_found";
    } else {
        output << position;
    }
    output << endl;
}

// Loads the regular User-Venue matrix for the Foursquare dataset from the specified directory.
inline SparseMatrix<RegularValue, RowMajor> loadRegularUserVenueMatrix(const fs::path& datasetsPath) {
    const fs::path matrixPath = datasetsPath / datasetName / "matrices" / (string(matrixName) + ".mtx");

    SparseMatrix<RegularValue, RowMajor> regularMatrix;
    if (!loadMatrix<RegularValue>(matrixPath, regularMatrix)) {
        throw runtime_error("Could not load regular User-Venue matrix: " + matrixPath.string());
    }

    regularMatrix.makeCompressed();
    return regularMatrix;
}

// Runs the analysis for the specified user node IDs, computing PathSim scores for both the regular and edge decomposition matrices, and returns the results as a formatted string.
template <typename SpatialValue>
string runAnalysis(const fs::path& datasetsPath,
                   const int firstUserNodeId,
                   const int secondUserNodeId,
                   const bool weighted) {
    validateUserNode(datasetsPath, firstUserNodeId);
    validateUserNode(datasetsPath, secondUserNodeId);
    ensureFoursquareBaseMatrixExists(datasetsPath);
    const int topK = loadNodeTypeCount(datasetsPath, userNodeType);
    if (topK <= 0) {
        throw runtime_error("No Foursquare user nodes found.");
    }

    SparseMatrix<RegularValue, RowMajor> regularMatrix = loadRegularUserVenueMatrix(datasetsPath);
    vector<RegularValue> regularDeltas = loadDeltas<RegularValue>(regularDeltaArtifactName(),
                                                                  datasetName,
                                                                  datasetsPath.string());
    const PathSimResult regularPathSim = computePathSimScore(regularMatrix,
                                                             regularDeltas,
                                                             firstUserNodeId,
                                                             secondUserNodeId);
    const int regularSecondPositionForFirstQuery = pathSimTopKPosition(regularMatrix,
                                                                       regularDeltas,
                                                                       firstUserNodeId,
                                                                       secondUserNodeId,
                                                                       topK);
    const int regularFirstPositionForSecondQuery = pathSimTopKPosition(regularMatrix,
                                                                       regularDeltas,
                                                                       secondUserNodeId,
                                                                       firstUserNodeId,
                                                                       topK);

    CreateRTreeIfMissing(datasetsPath.string(), datasetName, spatialAttributeFileName, delimiter);

    SparseMatrix<SpatialValue, RowMajor> spatialMatrix = loadEdgeDecompositionCommutingMatrix<SpatialValue>(datasetsPath.string(),
                                                                                                            datasetName,
                                                                                                            matrixName,
                                                                                                            spatialNodeName,
                                                                                                            spatialAttributeFileName,
                                                                                                            epsilonMeters,
                                                                                                            weighted,
                                                                                                            delimiter,
                                                                                                            true,
                                                                                                            true);
    spatialMatrix.makeCompressed();

    vector<SpatialValue> spatialDeltas = loadDeltas<SpatialValue>(edgeDecompositionArtifactName(weighted),
                                                                  datasetName,
                                                                  datasetsPath.string());
    const PathSimResult edgeDecompositionPathSim = computePathSimScore(spatialMatrix,
                                                                       spatialDeltas,
                                                                       firstUserNodeId,
                                                                       secondUserNodeId);
    const int edgeDecompositionSecondPositionForFirstQuery = pathSimTopKPosition(spatialMatrix,
                                                                                 spatialDeltas,
                                                                                 firstUserNodeId,
                                                                                 secondUserNodeId,
                                                                                 topK);
    const int edgeDecompositionFirstPositionForSecondQuery = pathSimTopKPosition(spatialMatrix,
                                                                                 spatialDeltas,
                                                                                 secondUserNodeId,
                                                                                 firstUserNodeId,
                                                                                 topK);

    ostringstream output;
    output << fixed << setprecision(12);

    output << "regular_row_dot_product = " << regularPathSim.dotProduct << endl;
    output << "regular_delta_" << firstUserNodeId << " = " << regularPathSim.firstDelta << endl;
    output << "regular_delta_" << secondUserNodeId << " = " << regularPathSim.secondDelta << endl;
    output << "regular_pathsim_score = " << regularPathSim.score << endl;
    appendRankingPosition(output, "regular", firstUserNodeId, secondUserNodeId, regularSecondPositionForFirstQuery);
    appendRankingPosition(output, "regular", secondUserNodeId, firstUserNodeId, regularFirstPositionForSecondQuery);
    output << endl;

    output << "edge_decomposition_row_dot_product = " << edgeDecompositionPathSim.dotProduct << endl;
    output << "edge_decomposition_delta_" << firstUserNodeId << " = " << edgeDecompositionPathSim.firstDelta << endl;
    output << "edge_decomposition_delta_" << secondUserNodeId << " = " << edgeDecompositionPathSim.secondDelta << endl;
    output << "edge_decomposition_pathsim_score = " << edgeDecompositionPathSim.score << endl;
    appendRankingPosition(output, "edge_decomposition", firstUserNodeId, secondUserNodeId, edgeDecompositionSecondPositionForFirstQuery);
    appendRankingPosition(output, "edge_decomposition", secondUserNodeId, firstUserNodeId, edgeDecompositionFirstPositionForSecondQuery);

    return output.str();
}

// Runs the main function for the Foursquare user PathSim score computation, handling command-line arguments and output.
template <typename SpatialValue>
int runMain(int argc,
            char* argv[],
            const bool weighted) {
    try {
        if (argc < 4 || argc > 5) {
            cerr << "Usage: " << argv[0] << " <Datasets path> <first_user_node_id> <second_user_node_id> [output_file]" << endl;
            return 1;
        }

        const fs::path datasetsPath = argv[1];
        const int firstUserNodeId = stoi(argv[2]);
        const int secondUserNodeId = stoi(argv[3]);

        if (firstUserNodeId < 0 || secondUserNodeId < 0) {
            throw runtime_error("User node IDs must be non-negative.");
        }

        const string output = runAnalysis<SpatialValue>(datasetsPath,
                                                        firstUserNodeId,
                                                        secondUserNodeId,
                                                        weighted);
        cout << output;

        if (argc == 5) {
            ofstream outputFile(argv[4]);
            if (!outputFile.is_open()) {
                throw runtime_error("Could not open output file: " + string(argv[4]));
            }
            outputFile << output;
        }

        return 0;
    } catch (const exception& exception) {
        cerr << "Error: " << exception.what() << endl;
        return 1;
    }
}

} // namespace foursquare_user_pathsim

#endif // FOURSQUARE_USER_PATHSIM_SCORES_HPP
