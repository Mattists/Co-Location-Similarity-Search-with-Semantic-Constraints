#ifndef PATHSIMQUERY_HPP
#define PATHSIMQUERY_HPP

#include <vector>
#include <set>
#include <unordered_set>
#include <utility>
#include <functional>
#include <Eigen/Sparse>

template <typename T = unsigned long long>
std::set<std::pair<double, int>, std::greater<>> PathSimTopKQuery(const Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix,
                                                                  const std::vector<T>& deltas,
                                                                  const Eigen::SparseVector<T, Eigen::RowMajor>& queryVector,
                                                                  const T queryDelta,
                                                                  const int k);

#endif // PATHSIMQUERY_HPP
