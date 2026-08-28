#include "SpatialMetaPathHelper.hpp"

#include "../DataHandler/MatrixIO.hpp"
#include "../DataHandler/utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <Eigen/Sparse>

using namespace Eigen;
using namespace std;
namespace fs = filesystem;

/**
 * Create an instance PathManager that holds the path and matrix names used to store the matrices created by the Spatially Extended Meta path Approach.
 * 
 * @param queryType The query type, at the moment this is always "RangeQuery".
 * @param matrixName The name of the meta path/commuting matrix to spatially extend.
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param attributeFileName The name of the file containing the spatial attributes for each spatial node used to perform the spatial extension.
 * @param paramStr A string containing the parameters, i.e. the k-value for a top-k query and "minRange_maxRange" for a range query.
 */
PathManager::PathManager(const string& queryType,
                         const string& matrixName,
                         const string& directory,
                         const string& datasetName,
                         const string& attributeFileName,
                         const string& paramStr) {
    nameAdd = queryType + paramStr;
    const fs::path datasetPath = fs::path(directory) / datasetName;
    matrixDirPath = datasetPath / "matrices";
    matrixPath = matrixDirPath / (matrixName + nameAdd + ".mtx");
    attributeFilePath = datasetPath / attributeFileName;
}

string PathManager::getNameAdd() const {
    return nameAdd;
}

fs::path PathManager::getMatrixDirPath() const {
    return matrixDirPath;
}

fs::path PathManager::getMatrixPath() const {
    return matrixPath;
}

fs::path PathManager::getAttributeFilePath() const {
    return attributeFilePath;
}

fs::path getRTreePath(const fs::path& datasetPath,
                      const string& attributeFileName) {

    string treeName = fs::path(attributeFileName).stem().string();
    treeName += "_RTree.dat";

    return datasetPath / treeName;
}


/**
 * Checks if a market or binary matrix artifact exists at the specified path.
 * 
 * @param matrixPath The path where the matrix should be checked.
 * @return True if the matrix artifact exists, false otherwise.
 */
static bool matrixArtifactExists(const fs::path& matrixPath) {
    fs::path binaryPath = matrixPath.string() + ".bin";
    return fs::exists(matrixPath) || fs::exists(binaryPath);
}


/**
 * Inverses a given commuting matrix/meta path name. Necessary if the inverse matrix is present instead of the requested matrix
 * 
 * @param matrixName The name of the commuting matrix/meta path to inverse.
 * @param nameByEdgeType A boolean that indicates whether the edges types are used for naming the matrices or whether abbreviations of the connected node types are used.
 * @return The inversed commuting matrix/meta path name.
 */
static string getInverseMatrixName(string matrixName,
                                   const bool nameByEdgeType) {
    if (nameByEdgeType) {
        if (matrixName.size() >= 3) {
            matrixName.erase(matrixName.size() - 3);
        } else {
            matrixName.clear();
        }
        if (!matrixName.empty() && matrixName.front() == '(' && matrixName.back() == ')') {
            matrixName = matrixName.substr(1, matrixName.length() - 2);
        }
    } else {
        reverse(matrixName.begin(), matrixName.end());
    }
    return matrixName;
}


/**
 * Reserves space for the specified number of H3 cells.
 * 
 * @param expectedCells The expected number of H3 cells to reserve space for.
 */
void H3ColumnMapper::reserve(const size_t expectedCells) {
    cellToColumnId.reserve(expectedCells);
    columnIdToCell.reserve(expectedCells);
}


/**
 * Returns the column ID in the spatially commuting matrix ("right matrix") for a given H3 cell. If the cell does not exist, a new column ID is created and returned.
 * 
 * @param cell The H3 cell for which the column ID should be retrieved or created.
 * @return The column ID for the given H3 cell.
 */
int H3ColumnMapper::getOrCreateColumnId(H3Index cell) {
    auto it = cellToColumnId.find(cell);
    if (it != cellToColumnId.end()) {
        return it->second;
    }

    if (columnIdToCell.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("Too many H3 cells for Eigen int column indices.");
    }

    const int columnId = static_cast<int>(columnIdToCell.size());
    cellToColumnId[cell] = columnId;
    columnIdToCell.push_back(cell);

    return columnId;
}


/**
 * Returns the number of unique H3 cells that have been mapped to column IDs in the spatially commuting matrix ("right matrix").
 * Not all H3 cells may have been mapped to column IDs, as some cells may not be spatially relevant to any spatial node.
 * 
 * @return The number of unique H3 cells that have been mapped to column IDs.
 */
size_t H3ColumnMapper::size() const {
    return columnIdToCell.size();
}

template <typename T>
void validateWeighted(const bool weighted) {
    static_assert(isSupportedScoreType<T>(), "The spatially extended meta path creation only supports unsigned long long and long double scores.");

    if constexpr (is_same_v<T, unsigned long long>) {
        if (weighted) {
            throw invalid_argument("unsigned long long scores can only be used with weighted=false.");
        }
    } else {
        if (!weighted) {
            throw invalid_argument("long double scores require weighted=true.");
        }
    }
}


/**
 * Computes the great-circle distance between two geographic coordinates using the Haversine formula.
 * 
 * The coordinates are expected to be given as longitude and latitude in decimal degrees.
 * @param coordinate1 The first geographic coordinate (longitude, latitude).
 * @param coordinate2 The second geographic coordinate (longitude, latitude).
 * @return The distance between the two coordinates in meters.
 */
double 
haversineDistanceMeters(const Coordinate& coordinate1,
                        const Coordinate& coordinate2) {
    constexpr double earthRadiusMeters = 6371008.8;
    const double lon1 = degreesToRadians(coordinate1[0]);
    const double lat1 = degreesToRadians(coordinate1[1]);
    const double lon2 = degreesToRadians(coordinate2[0]);
    const double lat2 = degreesToRadians(coordinate2[1]);

    const double dLon = lon2 - lon1;
    const double dLat = lat2 - lat1;
    const double sinHalfLon = sin(dLon / 2.0);
    const double sinHalfLat = sin(dLat / 2.0);
    const double a = sinHalfLat * sinHalfLat + cos(lat1) * cos(lat2) * sinHalfLon * sinHalfLon;
    const double clampedA = clamp(a, 0.0, 1.0);
    const double c = 2.0 * atan2(sqrt(clampedA), sqrt(1.0 - clampedA));

    return earthRadiusMeters * c;
}


/**
 * Computes the edge weight for a spatial edge connecting two objects with a given spatial distance.
 * 
 * @param distance The spatial distance between the two objects.
 * @param weighted Whether to use distance-based weighted edges. If false, every edge has weight 1. If true, the linear-decay weighting is used.
 * @param epsilonRange The range around a spatial node in that other spatial nodes will be considered as neighbors. Only relevant when weighted is true.
 * @return The computed edge weight.
 */
template <typename T>
T typedWeight(const double distance,
              const bool weighted,
              const int epsilonRange) {
    if constexpr (is_same_v<T, unsigned long long>) {
        return T{1};
    } else {
        return static_cast<T>(computeWeight(distance, weighted, epsilonRange));
    }
}


/** 
 * Computes the edge weight for a spatial edge connecting two objects with a given spatial distance.
 * 
 * @param distance The spatial distance between the two objects.
 * @param weighted Whether to use distance-based weighted edges. If false, every edge has weight 1. If true, the linear-decay weighting is used.
 * @param epsilonRange The range around a spatial node in that other spatial nodes will be considered as neighbors. Only relevant when weighted is true.
 * @return The computed edge weight.
 */
long double 
computeWeight(const double distance,
              const bool weighted,
              const int epsilonRange) {
    if (!weighted) {
        return 1.0L;
    }
    if (epsilonRange <= 0) {
        return distance <= 0.0 ? 1.0L : 0.0L;
    }
    return max(0.0L, 1.0L - (static_cast<long double>(distance) /
                             static_cast<long double>(epsilonRange)));
}


/**
 * Saves an R-Tree to a specified path.
 * 
 * @param rTree The R-Tree to save.
 * @param treePath The path where the R-Tree should be saved.   
 */
static void saveRTree(Tree& rTree,
                      const fs::path& treePath) {
                        
    fs::create_directories(treePath.parent_path());
    if (!rTree.Save(treePath.string().c_str())) {
        throw runtime_error("Could not save RTree to file " + treePath.string());
    }
}


/**
 * Loads an R-Tree to a specified path.
 * 
 * @param treePath The path where the R-Tree should be loaded from.
 * @return The loaded R-Tree.   
 */
static Tree loadRTreeFromPath(const fs::path& treePath) {

    Tree rTree;
    if (!rTree.Load(treePath.string().c_str())) {
        throw runtime_error("Could not load RTree from file " + treePath.string() +
                            ". Create and store it before running the query.");
    }
    return rTree;
}


/**
 * Creates and initializes an R-tree for DIMENSIONALITY-dimensional spatial data.
 * The file containing the data is assumed to hold the objects ID as first column,
 * followed by the DIMENSIONALITY-dimensional spatial information in the respective number of columns.
 *
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param attributeFileName The name of the file containing the spatial attributes used to create the RTree.
 * @param delimiter The delimiter used to separate the attributes in the file containing the spatial attributes.
 * @return The R-tree.
 */
Tree
CreateRTree(const string& directory,
            const string& datasetName,
            const string& attributeFileName,
            const char delimiter) {

    fs::path datasetPath = fs::path(directory) / datasetName;
    fs::path attributeFilePath = datasetPath / attributeFileName;
    ifstream attributeFileStream(attributeFilePath);

    if (!attributeFileStream.is_open()) throw runtime_error("Could not open file "+ attributeFilePath.string());

    Tree RTree;
    string line;
    while (getline(attributeFileStream, line)) {
        // Entry contains: id, DIMENSIONALITY * values
        vector<string> entry = split(line, delimiter);

        if (entry.size() == DIMENSIONALITY + 1) {
            try {
                int id = stoi(entry[0]);

                Coordinate coords;

                // Loop over all dimensions and convert the values directly into the coordinate
                for (size_t x = 0; x < DIMENSIONALITY; ++x) {
                    coords[x] = stod(entry[x + 1]);  // entry[0] is the ID, so start from entry[1] 
                }

                // Insert point at position -> min = max for points (different min and max values represent rectangles)
                RTree.Insert(coords.data(), coords.data(), id);
            } catch (const invalid_argument& e) {
                // cout << "Warning: Error occurred while converting values to double. Skipping line " << entry[0] << endl;
            } catch (const out_of_range& e) {
                cout << "Warning: Index out of range for entry. Skipping line " << entry[0] << endl;
            } catch (const exception& e) {
                cout << "Warning: An unexpected error occurred: " << e.what() << ". Skipping line " << entry[0] << endl;
            }
        }

    }
    saveRTree(RTree, getRTreePath(datasetPath, attributeFileName));
    return RTree;
}


/**
 * Checks if an R-tree exists for the given dataset and attribute file. If it does not exist, it creates a new R-tree using the specified parameters.
 * 
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param attributeFileName The name of the file containing the spatial attributes used to create the RTree.
 * @param delimiter The delimiter used to separate the attributes in the file containing the spatial attributes.
 */
void
CreateRTreeIfMissing(const string& directory,
                     const string& datasetName,
                     const string& attributeFileName,
                     const char delimiter) {

    fs::path datasetPath = fs::path(directory) / datasetName;
    if (!fs::exists(getRTreePath(datasetPath, attributeFileName))) {
        CreateRTree(directory, datasetName, attributeFileName, delimiter);
    }
}


/**
 * Loads an R-tree from the specified directory and dataset name. If the R-tree does not exist, it throws an exception.
 * 
 * @param directory The directory where the data is stored.
 * @param datasetName The name of the dataset.
 * @param attributeFileName The name of the file containing the spatial attributes used to create the RTree.
 */
Tree
LoadRTree(const string& directory,
          const string& datasetName,
          const string& attributeFileName) {

    fs::path datasetPath = fs::path(directory) / datasetName;
    return loadRTreeFromPath(getRTreePath(datasetPath, attributeFileName));
}


/**
 * This function is used when querying the RTree.
 * Adapt it to handle each processed data point, e.g. print its id.
 * @return True.
 */
bool MySearchCallback(int id)
{
    //cout << id << endl;
    return true;
}


/**
 * Processes a line read from a file. The line is parsed into a variable for the id and a Coordinate for the attributes.
 * 
 * @param id Variable to store the id of the processed line.
 * @param coords Coordinate to store the spatial values of the processed line.
 * @param line The actual line to process.
 * @param delimiter The delimiter used to separate the spatial attributes in the line.
 * @return A Boolean indicating whether the line has been parsed successfully.
 */
bool processLine(int& id, 
                 Coordinate& coords,
                 const string& line,
                 const char delimiter) {

    // Entry contains: id, DIMENSIONALITY * values
    vector<string> entry = split(line, delimiter);

    if (entry.size() != DIMENSIONALITY + 1) {
        return false;
    }

    for (size_t x = 1; x <= DIMENSIONALITY; ++x) {
        if (entry[x].empty()) {
            return false;
        }
    }

    try {
        id = stoi(entry[0]);

        // Loop over all dimensions and convert the values directly into the coordinate
        for (size_t x = 0; x < DIMENSIONALITY; ++x) {
            coords[x] = stod(entry[x + 1]); // entry[0] is the ID, so we start from entry[1]
        }
        return true;
    } catch (const invalid_argument& e) {
        // cout << "Warning: Error occurred while converting values to double. Skipping line " << entry[0] << endl;
        return false;
    } catch (const out_of_range& e) {
        cout << "Warning: Index out of range for entry. Skipping line " << entry[0] << endl;
        return false;
    } catch (const exception& e) {
        cout << "Warning: An unexpected error occurred: " << e.what() << ". Skipping line " << entry[0] << endl;
        return false;
    }    
}

/**
 * Loads the commuting matrix that should be extended to a new spatially meta path, i.e. the matrix containing all edges except for the last, spatially one.
 * This commuting matrix is always assumed to be present and is not changed, while the right matrix has to be newly created.
 * 
 * @param matrixName The name of the matrix/meta path to load.
 * @param matrixDirPath The path where the matrix files are stored.
 * @param nameByEdgeType A boolean that indicates whether the edges types are used for naming the matrices or whether abbreviations of the connected node types are used.
 * @return The matrix to load.
 */
SparseMatrix<unsigned long long, RowMajor> loadLeftMatrix(string matrixName,
                                                          const fs::path& matrixDirPath, 
                                                          const bool nameByEdgeType) {
    const fs::path requestedMatrixPath = matrixDirPath / (matrixName + ".mtx");
    fs::path matrixToLoadPath;
    bool transposeAfterLoad;

    if (matrixArtifactExists(requestedMatrixPath)) {
        matrixToLoadPath = requestedMatrixPath;
        transposeAfterLoad = false;
    } else {
        matrixToLoadPath = matrixDirPath / (getInverseMatrixName(matrixName, nameByEdgeType) + ".mtx");
        transposeAfterLoad = true;
    }

    SparseMatrix<unsigned long long, RowMajor> leftMatrix;
    if (!loadMatrix<unsigned long long>(matrixToLoadPath, leftMatrix)) {
        throw runtime_error("Can not open commuting matrix file: " + requestedMatrixPath.string() +
                            " or inverse matrix file: " + matrixToLoadPath.string());
    }

    if (transposeAfterLoad) {
        leftMatrix = leftMatrix.transpose();
        leftMatrix.makeCompressed();
    }

    return leftMatrix;
}

// ----------------------------------- Instantiation of template functions --------------------------

template void validateWeighted<unsigned long long>(const bool);

template void validateWeighted<long double>(const bool);

template unsigned long long typedWeight<unsigned long long>(const double,
                                                            const bool,
                                                            const int);

template long double typedWeight<long double>(const double,
                                              const bool,
                                              const int);
