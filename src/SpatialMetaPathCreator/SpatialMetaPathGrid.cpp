#include "SpatialMetaPathGrid.hpp"

#include "SpatialMetaPathHelper.hpp"
#include "../DataHandler/MatrixIO.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>

extern "C" {
#include <h3api.h>
}

using namespace Eigen;
using namespace std;

/**
 *
 * Function to load or create the commuting matrix for a grid-based spatially extended meta path for PathSim computation.
 * The meta path is created using a range query to extend an existing matrix by multiplication with a spatial matrix connecting spatial nodes to grid cells.
 * Note that only the spatial matrix (in the right-most position of the left half of the meta path) is created using this function. 
 * The previous part of the resulting meta path is assumed to be already present.
 * Further note that the more general case where the proximity-based virtual edge is in the middle of the meta path is considered here as this meta path can
 * simply be split up into two parts where the left part up to the virtual edge is considered here. The returned matrix is then multiplied with the right part
 * to retrieve the full commuting matrix needed for PathSim computation.
 *
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param matrixName The name of the meta path/commuting matrix to spatially extend, assumed to be present. If not present, it is assumed that its inverse is present.
 *                   If the inverse is present, the left matrix is denoted leftMatrix^-1 for a single adjacency matrix and (leftMatrix)^-1
 *                   for a more complex commuting matrix when edges types are used for naming (nameByEdgeType set True), otherwise the name is simply inverted.
 * @param spatialNodeName The name of the spatial node type used in for the spatial extension. This has to be the right-most type in the meta path matrixName.
 * @param attributeFileName The name of the file containing the spatial attributes for each spatial node used to perform the spatial extension.
 * @param epsilonRange The range (in meters) around a spatial node in that grid cells will be considered as neighbors.
 * @param weighted Whether to use distance-based weighted edges. If false, every edge has weight 1. If true, the linear-decay weighting is used.
 * @param delimiter The delimiter used to separate the attributes in the file containing the spatial attributes.
 * @param nameByEdgeType A boolean that indicates whether the edges types are used for naming the matrices or whether abbreviations of the connected node types are used.
 * @param shortDeltaName A boolean that indicates whether the short name (only first half of round-trip meta path) should be used to name the delta file. 
 *                       This might be necessary as there is a limit on the length of the file name in some file systems.
 * @param gridResolution The resolution of the H3 grid used for the range query. The higher the resolution, the more fine-grained the grid and thus potentially more accurate but also more computationally expensive. 
 *                       Must be between 0 and 15 for the H3 grid, where values larger than 7/8 are not recommended due to high computational effort and a too fine-grained resolution.
 * @param saveMatrixMarketFile A boolean that indicates whether the matrix market file should be saved or if the binary file is sufficient.
 * @param saveDeltaTextFile A boolean that indicates whether the delta text file should be saved or if the binary file is sufficient.
 * @return The commuting matrix representing the spatially extended meta path created.
 */
template <typename T>
SparseMatrix<T, RowMajor> loadGridCommutingMatrix(const string& directory,
                                                  const string& datasetName,
                                                  const string& matrixName,
                                                  const string& spatialNodeName,
                                                  const string& attributeFileName,
                                                  const int epsilonRange,
                                                  const bool weighted,
                                                  const char delimiter,
                                                  const bool nameByEdgeType,
                                                  const bool shortDeltaName,
                                                  const int gridResolution,
                                                  const bool saveMatrixMarketFile,
                                                  const bool saveDeltaTextFile) {

    static_assert(isSupportedScoreType<T>(), "The spatially extended meta path creation only supports unsigned long long and long double scores.");
    validateWeighted<T>(weighted);

    const string newMatrixName = matrixName + '#' + spatialNodeName + "_Grid";
    PathManager pm("RangeQuery" + weightedName(weighted),
                   newMatrixName,
                   directory,
                   datasetName,
                   attributeFileName,
                   to_string(epsilonRange) + '_' + to_string(gridResolution));

    SparseMatrix<T, RowMajor> existingMatrix;
    if (loadMatrix<T>(pm.getMatrixPath(), existingMatrix)) {
        return existingMatrix;
    }

    SparseMatrix<unsigned long long, RowMajor> leftMatrix = loadLeftMatrix(matrixName,
                                                                           pm.getMatrixDirPath(),
                                                                           nameByEdgeType);
    leftMatrix.makeCompressed();
    const size_t expectedSpatialNodeCount = static_cast<size_t>(leftMatrix.cols());
    
    if (gridResolution < 0 || gridResolution > 15) {
        throw invalid_argument("The H3 grid resolution must be between 0 and 15.");
    }

    if (epsilonRange < 0) {
        throw invalid_argument("Invalid epsilon range.");
    }

    vector<Triplet<T>> triplets;
    triplets.reserve(expectedSpatialNodeCount);
    H3ColumnMapper h3ColumnMapper;
    h3ColumnMapper.reserve(expectedSpatialNodeCount);
    unordered_map<H3Index, Coordinate> cellCenterCache;
    cellCenterCache.reserve(expectedSpatialNodeCount);
    
    using CandidateCell = pair<H3Index, Coordinate>;
    using CellNeighborhood = vector<CandidateCell>;
    unordered_map<H3Index, CellNeighborhood> cellNeighborhoodCache;
    cellNeighborhoodCache.reserve(expectedSpatialNodeCount);

    double avgEdgeLengthMeters;
    H3Error error = getHexagonEdgeLengthAvgM(gridResolution, &avgEdgeLengthMeters);
    if (error != E_SUCCESS) {
        throw runtime_error("Could not get average H3 edge length.");
    }

    // Conservative choice of k to ensure that the grid disk covers the entire epsilon range.
    const int k = static_cast<int>(ceil(static_cast<double>(epsilonRange) / avgEdgeLengthMeters)) + 2;

    int64_t maxDiskSize;
    error = maxGridDiskSize(k, &maxDiskSize);
    if (error != E_SUCCESS) {
        throw runtime_error("Could not compute maximum H3 grid disk size.");
    }
    vector<H3Index> candidateCells(static_cast<size_t>(maxDiskSize));
   
    // Returns cached H3 cell centers so a candidate cell's coordinate conversion happens at most once.
    auto getCellCenter = [&](const H3Index candidateCell) -> const Coordinate& {
        auto cellCenterIt = cellCenterCache.find(candidateCell);
        if (cellCenterIt != cellCenterCache.end()) {
            return cellCenterIt->second;
        }

        LatLng cellCenter;
        error = cellToLatLng(candidateCell, &cellCenter);
        if (error != E_SUCCESS) {
            throw runtime_error("Could not compute H3 cell center.");
        }

        Coordinate cellCenterCoords {};
        cellCenterCoords[0] = radsToDegs(cellCenter.lng);
        cellCenterCoords[1] = radsToDegs(cellCenter.lat);
        return cellCenterCache.emplace(candidateCell, cellCenterCoords).first->second;
    };

    // Builds each center cell's grid disk once; per-object distances and weights are still computed below.
    auto getCellNeighborhood = [&](const H3Index centerCell) -> const CellNeighborhood& {
        auto cellNeighborhoodIt = cellNeighborhoodCache.find(centerCell);
        if (cellNeighborhoodIt != cellNeighborhoodCache.end()) {
            return cellNeighborhoodIt->second;
        }

        fill(candidateCells.begin(), candidateCells.end(), H3Index{0});
        error = gridDisk(centerCell, k, candidateCells.data());
        if (error != E_SUCCESS) {
            throw runtime_error("Could not compute H3 grid disk.");
        }

        CellNeighborhood neighborhood;
        neighborhood.reserve(candidateCells.size());
        for (const H3Index candidateCell : candidateCells) {
            if (candidateCell == 0) {
                continue;
            }

            neighborhood.emplace_back(candidateCell, getCellCenter(candidateCell));
        }

        return cellNeighborhoodCache.emplace(centerCell, move(neighborhood)).first->second;
    };

    ifstream attributeFileStream(pm.getAttributeFilePath());
    if (!attributeFileStream.is_open()) {
        throw runtime_error("Could not open file " + pm.getAttributeFilePath().string());
    }

    string line;
    while (getline(attributeFileStream, line)) {
        int id;
        Coordinate coords;

        if (!processLine(id, coords, line, delimiter)) {
            continue;
        }

        if (id < 0 || id >= leftMatrix.cols()) {
            throw out_of_range("Spatial node id is outside the column range of the left matrix.");
        }

        LatLng queryLatLng;
        queryLatLng.lng = degsToRads(coords[0]);
        queryLatLng.lat = degsToRads(coords[1]);

        H3Index centerCell;
        error = latLngToCell(&queryLatLng, gridResolution, &centerCell);
        if (error != E_SUCCESS) {
            throw runtime_error("Could not convert coordinate to H3 cell.");
        }

        const CellNeighborhood& cellNeighborhood = getCellNeighborhood(centerCell);
        for (const auto& [candidateCell, cellCenter] : cellNeighborhood) {
            const double distance = haversineDistanceMeters(coords, cellCenter);
            if (distance > epsilonRange) {
                continue;
            }

            const T edgeWeight = typedWeight<T>(distance, weighted, epsilonRange);
            if (edgeWeight != T{}) {
                const int columnId = h3ColumnMapper.getOrCreateColumnId(candidateCell);
                triplets.emplace_back(id, columnId, edgeWeight);
            }
        }
    }

    const int gridPointCount = static_cast<int>(h3ColumnMapper.size());

    SparseMatrix<T, RowMajor> rightMatrix(leftMatrix.cols(), gridPointCount);
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

template SparseMatrix<unsigned long long, RowMajor> loadGridCommutingMatrix<unsigned long long>(const string&,
                                                                                                const string&,
                                                                                                const string&,
                                                                                                const string&,
                                                                                                const string&,
                                                                                                const int,
                                                                                                const bool,
                                                                                                const char,
                                                                                                const bool,
                                                                                                const bool,
                                                                                                const int,
                                                                                                const bool,
                                                                                                const bool);

template SparseMatrix<long double, RowMajor> loadGridCommutingMatrix<long double>(const string&,
                                                                                  const string&,
                                                                                  const string&,
                                                                                  const string&,
                                                                                  const string&,
                                                                                  const int,
                                                                                  const bool,
                                                                                  const char,
                                                                                  const bool,
                                                                                  const bool,
                                                                                  const int,
                                                                                  const bool,
                                                                                  const bool);
