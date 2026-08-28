#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Sparse>

#include "DataHandler/MatrixIO.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathGrid.hpp"

namespace fs = std::filesystem;
using namespace Eigen;
using namespace std;

const string datasetName = "GeoDBLP";
const string matrixName = "published_in_conference^-1#wrote^-1#affiliated_with";
const string spatialAttributeName = "affiliation";
const string attributeFileName = "affiliationsWGS84.csv";
constexpr char delimiter = ',';

struct NeighborCount {
    int conference;
    int directNeighbors;
    int spatialNeighbors;
};

template <typename T>
int rowSum(const SparseMatrix<T, RowMajor>& matrix,
           const int row) {
    int sum = 0;
    for (typename SparseMatrix<T, RowMajor>::InnerIterator it(matrix, row); it; ++it) {
        sum += 1;
    }
    return sum;
}

template <typename T>
vector<NeighborCount> collectNeighborCounts(const SparseMatrix<unsigned long long, RowMajor>& directMatrix,
                                            const SparseMatrix<T, RowMajor>& spatialMatrix) {
    vector<NeighborCount> counts;
    counts.reserve(directMatrix.rows());

    for (int row = 0; row < directMatrix.rows(); row++) {
        counts.push_back({
            row,
            rowSum(directMatrix, row),
            rowSum(spatialMatrix, row)
        });
    }

    sort(counts.begin(), counts.end(), [](const NeighborCount& left, const NeighborCount& right) {
        if (left.directNeighbors != right.directNeighbors) {
            return left.directNeighbors < right.directNeighbors;
        }
        if (left.spatialNeighbors != right.spatialNeighbors) {
            return left.spatialNeighbors < right.spatialNeighbors;
        }
        return left.conference < right.conference;
    });

    return counts;
}

void printCounts(const string& approach,
                 const int epsilonSize,
                 const int gridResolution,
                 const vector<NeighborCount>& counts) {
    for (const NeighborCount& count : counts) {
        cout << approach << ";"
             << epsilonSize << ";"
             << gridResolution << ";"
             << count.conference << ";"
             << count.directNeighbors << ";"
             << count.spatialNeighbors << endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: countSpatialNeighbors <Datasets path>" << endl;
        return 1;
    }

    fs::path allDatasetPath = argv[1];

    vector<int> epsilonSizes = {4000,16000,64000,128000};
    vector<int> gridSizes = {2,4,6};

    SparseMatrix<unsigned long long, RowMajor> directMatrix;
    const fs::path directMatrixPath = allDatasetPath / datasetName / "matrices" / (matrixName + ".mtx");
    if (!loadMatrix<unsigned long long>(directMatrixPath, directMatrix)) {
        throw runtime_error("Can not open commuting matrix file: " + directMatrixPath.string());
    }

    cout << "approach;epsilon;grid_resolution;conference;direct_neighbors;spatial_neighbors" << endl;

    for (int epsilonSize : epsilonSizes) {
        for (int gridSize : gridSizes) {

            SparseMatrix<unsigned long long, RowMajor> gridMatrix = loadGridCommutingMatrix(allDatasetPath.string(),
                                                                                            datasetName,
                                                                                            matrixName,
                                                                                            spatialAttributeName,
                                                                                            attributeFileName,
                                                                                            epsilonSize,
                                                                                            false,
                                                                                            delimiter,
                                                                                            true,
                                                                                            true,
                                                                                            gridSize);
                    
            printCounts("grid",
                        epsilonSize,
                        gridSize,
                        collectNeighborCounts(directMatrix, gridMatrix));
        }
    }
}
