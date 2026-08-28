#include "SpatialMetaPathEdgeDecomposition.hpp"

#include "SpatialMetaPathHelper.hpp"
#include "../DataHandler/MatrixIO.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>
#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>

using namespace Eigen;
using namespace std;

namespace {

/**
 * Returns a unique key for a pair of spatial node ids. The key is constructed by packing the two 32-bit node ids into a single 64-bit value, with the first node id in the higher 32 bits and the second node id in the lower 32 bits.
 *  Node ids are validated as non-negative ints before this is called, so packing two 32-bit ids into one 64-bit value gives each unordered pair a unique key.
 * @param firstNode The id of the first spatial node.
 * @param secondNode The id of the second spatial node.
 * @return A unique key for the pair of spatial node ids.
 */
uint64_t spatialNodePairKey(const int firstNode,
                            const int secondNode) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(firstNode)) << 32U) |
           static_cast<uint32_t>(secondNode);
}

/**
 * Returns the edge weight for a given direct distance between two spatial nodes, based on the specified weighting scheme and epsilon range.
 * If the weighting scheme is unweighted, the edge weight is always 1. If the weighting scheme is weighted, the edge weight is computed as the square root of the direct distance-based weight.
 * @param directDistance The direct distance between the two spatial nodes.
 * @param weighted A boolean indicating whether the edge weight should be computed using a weighted scheme (true) or an unweighted scheme (false).
 * @param epsilonRange The epsilon range used for the linear decay function when the weighting scheme is weighted. This parameter is ignored when the weighting scheme is unweighted.
 * @return The computed edge weight for the given direct distance, based on the specified weighting scheme and epsilon range.
 */
template <typename T>
T edgeDecompositionEdgeWeight(const double directDistance,
                              const bool weighted,
                              const int epsilonRange) {
    const T directWeight = typedWeight<T>(directDistance, weighted, epsilonRange);
    if (directWeight == T{}) {
        return T{};
    }

    if constexpr (is_same_v<T, unsigned long long>) {
        return directWeight;
    } else {
        // The edge-decomposition path replaces one direct spatial edge with two half-edges. Using the
        // square root on both half-edges keeps their product equal to the direct
        // distance-based edge weight.
        return static_cast<T>(sqrt(static_cast<long double>(directWeight)));
    }
}

} // namespace


/**
 *
 * Function to load or create the commuting matrix for an edge-decomposition-based spatially extended meta path for PathSim computation.
 * The meta path is created using a range query. For every spatial node pair within the epsilon range, this variant creates
 * one EdgeDecomposition object and connects both spatial nodes of the pair to that object. This preserves a symmetric round-trip center:
 * the path node -> EdgeDecomposition -> node has the same weight as the direct distance-based spatial edge because both half-edges
 * use the square root of the direct weight.
 *
 * Note that only the spatial EdgeDecomposition matrix (in the right-most position of the left half of the meta path) is created using
 * this function. The previous part of the resulting meta path is assumed to be already present.
 *
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param matrixName The name of the meta path/commuting matrix to spatially extend, assumed to be present. If not present, it is assumed that its inverse is present.
 *                   If the inverse is present, the left matrix is denoted leftMatrix^-1 for a single adjacency matrix and (leftMatrix)^-1
 *                   for a more complex commuting matrix when edges types are used for naming (nameByEdgeType set True), otherwise the name is simply inverted.
 * @param spatialNodeName The name of the spatial node type used in for the spatial extension. This has to be the right-most type in the meta path matrixName.
 * @param attributeFileName The name of the file containing the spatial attributes for each spatial node used to perform the spatial extension.
 * @param epsilonRange The range (in meters) around a spatial node in that other spatial nodes will be considered as neighbors.
 * @param weighted Whether to use distance-based weighted edges. If false, every edge has weight 1. If true, the linear-decay weighting is used.
 * @param delimiter The delimiter used to separate the attributes in the file containing the spatial attributes.
 * @param nameByEdgeType A boolean that indicates whether the edges types are used for naming the matrices or whether abbreviations of the connected node types are used.
 * @param shortDeltaName A boolean that indicates whether the short name (only first half of round-trip meta path) should be used to name the delta file.
 *                       This might be necessary as there is a limit on the length of the file name in some file systems.
 * @param saveMatrixMarketFile A boolean that indicates whether the matrix market file should be saved or if the binary file is sufficient.
 * @param saveDeltaTextFile A boolean that indicates whether the delta text file should be saved or if the binary file is sufficient.
 * @return The commuting matrix representing the edge-decomposition-based spatially extended meta path created.
 */
template <typename T>
SparseMatrix<T, RowMajor> loadEdgeDecompositionCommutingMatrix(const string& directory,
                                                   const string& datasetName,
                                                   const string& matrixName,
                                                   const string& spatialNodeName,
                                                   const string& attributeFileName,
                                                   const int epsilonRange,
                                                   const bool weighted,
                                                   const char delimiter,
                                                   const bool nameByEdgeType,
                                                   const bool shortDeltaName,
                                                   const bool saveMatrixMarketFile,
                                                   const bool saveDeltaTextFile) {

    static_assert(isSupportedScoreType<T>(), "The spatially extended meta path creation only supports unsigned long long and long double scores.");
    validateWeighted<T>(weighted);

    const string newMatrixName = matrixName + '#' + spatialNodeName + "_EdgeDecomposition";
    PathManager pm("RangeQuery" + weightedName(weighted),
                   newMatrixName,
                   directory,
                   datasetName,
                   attributeFileName,
                   to_string(epsilonRange));

    SparseMatrix<T, RowMajor> existingMatrix;
    if (loadMatrix<T>(pm.getMatrixPath(), existingMatrix)) {
        return existingMatrix;
    }

    SparseMatrix<unsigned long long, RowMajor> leftMatrix = loadLeftMatrix(matrixName,
                                                                           pm.getMatrixDirPath(),
                                                                           nameByEdgeType);
    leftMatrix.makeCompressed();

    if (leftMatrix.cols() > static_cast<Index>(numeric_limits<int>::max())) {
        throw overflow_error("Too many spatial nodes for int-based R-tree ids.");
    }

    if (epsilonRange < 0) {
        throw invalid_argument("Invalid epsilon range.");
    }

    vector<Triplet<T>> triplets;
    triplets.reserve(static_cast<size_t>(leftMatrix.cols()) * 2U);

    unordered_set<uint64_t> edgeDecompositionPairs;
    edgeDecompositionPairs.reserve(static_cast<size_t>(leftMatrix.cols()));

    ifstream attributeFileStream(pm.getAttributeFilePath());
    if (!attributeFileStream.is_open()) {
        throw runtime_error("Could not open file " + pm.getAttributeFilePath().string());
    }

    Tree RTree = LoadRTree(directory, datasetName, attributeFileName);

    int edgeDecompositionColumnCount = 0;
    string line;
    while (getline(attributeFileStream, line)) {
        int id;
        Coordinate coords;

        if (processLine(id, coords, line, delimiter)) {
            vector<pair<double, int>> neighbors = RTree.RangeQuery(coords.data(),
                                                                coords.data(),
                                                                epsilonRange,
                                                                MySearchCallback);

            for (const pair<double, int>& neighbor: neighbors) {
                const int neighborId = neighbor.second;

                const int firstNode = min(id, neighborId);
                const int secondNode = max(id, neighborId);
                const auto insertResult = edgeDecompositionPairs.insert(spatialNodePairKey(firstNode, secondNode));
                if (!insertResult.second) { // The pair already has an EdgeDecomposition column.
                    continue;
                }

                if (edgeDecompositionColumnCount == numeric_limits<int>::max()) {
                    throw overflow_error("Too many EdgeDecomposition nodes for Eigen int column indices. Choose smaller epsilon for this dataset.");
                }

                const T edgeWeight = edgeDecompositionEdgeWeight<T>(neighbor.first, weighted, epsilonRange);
                if (edgeWeight != T{}) {
                    const int edgeDecompositionColumn = edgeDecompositionColumnCount++;
                    triplets.emplace_back(firstNode, edgeDecompositionColumn, edgeWeight);
                    if (secondNode != firstNode) {
                        triplets.emplace_back(secondNode, edgeDecompositionColumn, edgeWeight);
                    }
                }
            }
        }
    }

    SparseMatrix<T, RowMajor> rightMatrix(leftMatrix.cols(), edgeDecompositionColumnCount);
    rightMatrix.setFromTriplets(triplets.begin(), triplets.end());
    rightMatrix.makeCompressed();

    SparseMatrix<T, RowMajor> sparseMatrix = leftMatrix.cast<T>() * rightMatrix;
    sparseMatrix.makeCompressed();

    if (saveMatrixMarketFile) {
        saveMarket(sparseMatrix, pm.getMatrixPath());
    }
    saveMatrixBinary(sparseMatrix, pm.getMatrixPath());

    createAndStoreDeltas<T>(sparseMatrix,
                            newMatrixName,
                            pm.getMatrixDirPath(),
                            nameByEdgeType,
                            pm.getNameAdd(),
                            shortDeltaName,
                            saveDeltaTextFile);

    return sparseMatrix;
}

template SparseMatrix<unsigned long long, RowMajor> loadEdgeDecompositionCommutingMatrix<unsigned long long>(const string&,
                                                                                                 const string&,
                                                                                                 const string&,
                                                                                                 const string&,
                                                                                                 const string&,
                                                                                                 const int,
                                                                                                 const bool,
                                                                                                 const char,
                                                                                                 const bool,
                                                                                                 const bool,
                                                                                                 const bool,
                                                                                                 const bool);

template SparseMatrix<long double, RowMajor> loadEdgeDecompositionCommutingMatrix<long double>(const string&,
                                                                                   const string&,
                                                                                   const string&,
                                                                                   const string&,
                                                                                   const string&,
                                                                                   const int,
                                                                                   const bool,
                                                                                   const char,
                                                                                   const bool,
                                                                                   const bool,
                                                                                   const bool,
                                                                                   const bool);
