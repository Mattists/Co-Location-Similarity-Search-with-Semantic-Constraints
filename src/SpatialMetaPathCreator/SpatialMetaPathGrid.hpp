#ifndef SPATIALMETAPATH_GRID_HPP
#define SPATIALMETAPATH_GRID_HPP

#include <string>
#include <Eigen/Sparse>
#include "SpatialMetaPathHelper.hpp"

template <typename T = unsigned long long>
Eigen::SparseMatrix<T, Eigen::RowMajor> loadGridCommutingMatrix(const std::string& directory,
                                                                const std::string& datasetName,
                                                                const std::string& matrixName,
                                                                const std::string& spatialNodeName,
                                                                const std::string& attributeFileName,
                                                                const int epsilonRange,
                                                                const bool weighted,
                                                                const char delimiter = ',',
                                                                const bool nameByEdgeType = false,
                                                                const bool shortDeltaName = false,
                                                                const int gridResolution = 5,
                                                                const bool saveMatrixMarketFile = false,
                                                                const bool saveDeltaTextFile = false);

#endif //SPATIALMETAPATH_GRID_HPP
