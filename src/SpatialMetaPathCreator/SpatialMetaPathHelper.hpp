#ifndef SPATIALMETAPATH_HELPER_HPP
#define SPATIALMETAPATH_HELPER_HPP

#include <Eigen/Sparse>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <limits>
#include <cstdint>
#include <type_traits>
#include "../RTree/RTree.h"

namespace fs = std::filesystem;

using H3Index = uint64_t;

// Global constant for the spatial dimensionality (could be different to 2 if more general feature spaces are used)
constexpr std::size_t DIMENSIONALITY = 2;

using Coordinate = std::array<double, DIMENSIONALITY>;

// Class that holds the path and matrix names
class PathManager {
private:
    std::string nameAdd;
    fs::path matrixDirPath;
    fs::path matrixPath;
    fs::path attributeFilePath;

public:
    PathManager(const std::string& queryType,
                const std::string& matrixName, 
                const std::string& directory, 
                const std::string& datasetName,
                const std::string& attributeFileName, 
                const std::string& paramStr = "");

    std::string getNameAdd() const;
    fs::path getMatrixDirPath() const;
    fs::path getMatrixPath() const;
    fs::path getAttributeFilePath() const;
};

// Class that maps H3 cells to column IDs in the matrix
class H3ColumnMapper {
public:
    void reserve(size_t expectedCells);

    int getOrCreateColumnId(H3Index cell);

    size_t size() const;

private:
    std::unordered_map<H3Index, int> cellToColumnId;
    std::vector<H3Index> columnIdToCell;
};

// Define the R-tree type. It stores IDs of type int together with a specified number (DIMENSIONALITY) spatial features of type double.
typedef RTree<int, double, DIMENSIONALITY> Tree;

// Return the matrix/artifact name suffix for the binary weighted indicator.
inline std::string weightedName(const bool weighted) {
    return weighted ? "Weighted" : "Unweighted";
}

// Convert degrees to radians
inline double degreesToRadians(const double degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

double 
haversineDistanceMeters(const Coordinate& left,
                        const Coordinate& right);

template <typename T>
constexpr bool isSupportedScoreType() {
    return std::is_same_v<T, unsigned long long> || std::is_same_v<T, long double>;
}

template <typename T>
void validateWeighted(const bool weighted);

template <typename T>
T typedWeight(const double distance,
              const bool weighted,
              const int epsilonRange);

long double
computeWeight(const double distance,
              const bool weighted,
              const int epsilonRange = 0);

Tree
CreateRTree(const std::string& directory,
            const std::string& datasetName,
            const std::string& attributeFileName,
            const char delimiter = ',');

void
CreateRTreeIfMissing(const std::string& directory,
                     const std::string& datasetName,
                     const std::string& attributeFileName,
                     const char delimiter = ',');

Tree
LoadRTree(const std::string& directory,
          const std::string& datasetName,
          const std::string& attributeFileName);

fs::path getRTreePath(const fs::path& datasetPath,
                      const std::string& attributeFileName);

bool
MySearchCallback(int id);

bool processLine(int& id, 
                 Coordinate& coords,
                 const std::string& line,
                 const char delimiter = ',');


Eigen::SparseMatrix<unsigned long long, Eigen::RowMajor> loadLeftMatrix(std::string matrixName,
                                                                        const fs::path& matrixDirPath, 
                                                                        const bool nameByEdgeType = false);

#endif //SPATIALMETAPATH_HELPER_HPP
