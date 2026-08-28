#ifndef SPATIALMETAPATH_EDGE_DECOMPOSITION_HPP
#define SPATIALMETAPATH_EDGE_DECOMPOSITION_HPP

#include <string>
#include <Eigen/Sparse>
#include "SpatialMetaPathHelper.hpp"

template <typename T = unsigned long long>
Eigen::SparseMatrix<T, Eigen::RowMajor> loadEdgeDecompositionCommutingMatrix(const std::string& directory,
                                                                             const std::string& datasetName,
                                                                             const std::string& matrixName,
                                                                             const std::string& spatialNodeName,
                                                                             const std::string& attributeFileName,
                                                                             const int epsilonRange,
                                                                             const bool weighted,
                                                                             const char delimiter = ',',
                                                                             const bool nameByEdgeType = false,
                                                                             const bool shortDeltaName = false,
                                                                             const bool saveMatrixMarketFile = false,
                                                                             const bool saveDeltaTextFile = false);

#endif //SPATIALMETAPATH_EDGE_DECOMPOSITION_HPP
