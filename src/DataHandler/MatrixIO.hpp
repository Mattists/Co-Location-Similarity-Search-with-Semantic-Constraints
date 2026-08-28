#ifndef MATRIXIO_HPP
#define MATRIXIO_HPP

#include <filesystem>
#include <string>
#include <vector>

#include <Eigen/Sparse>

template <typename T = unsigned long long>
bool loadMatrix(const std::filesystem::path& matrixPath,
                Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix);

template <typename T = unsigned long long>
void saveMatrixBinary(const Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix,
                      const std::filesystem::path& matrixPath);

template <typename T = unsigned long long>
bool loadMatrixBinary(const std::filesystem::path& matrixPath,
                      Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix);

template <typename T = unsigned long long>
void saveDeltasBinary(const std::vector<T>& deltas,
                      const std::filesystem::path& deltaPath);

template <typename T = unsigned long long>
bool loadDeltasBinary(const std::filesystem::path& deltaPath,
                      std::vector<T>& deltas);

template <typename T = unsigned long long>
void createAndStoreDeltas(const Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix,
                          const std::string& matrixName,
                          const std::string& directory,
                          const bool nameByEdgeType = false,
                          const std::string& matrixNameAddition = "",
                          const bool shortName = false,
                          const bool saveDeltaTextFile = false);

template <typename T = unsigned long long>
std::vector<T> loadDeltas(const std::string& matrixName,
                          const std::string& datasetName,
                          const std::string& directory);

#endif // MATRIXIO_HPP
