#include "PathSimQuery.hpp"

#include <functional>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Eigen/Sparse>

using namespace std;
using namespace Eigen;


/**
 * Calculate the one step neighbor given a sparse matrix and a query object. This is essentially just all the columns
 * where the query object has non zero entries.
 *
 * @param sparseMatrix The given sparse matrix.
 * @param queryObject The index of the query object.
 * @return A candidate list containing indices.
 */
template <typename T>
unordered_set<int> oneStepNeighbor(const SparseVector<T, RowMajor>& vector) {
    unordered_set<int> candidateList;
    for (typename SparseVector<T, RowMajor>::InnerIterator it(vector); it; ++it) {
        candidateList.insert((int) it.col());
    }
    return candidateList;
}


/**
 * Calculate the two step neighbor given a sparse matrix and a query object. This is essentially just all the objects
 * that have overlapping indices with the query object.
 *
 * @param sparseMatrix The given sparse matrix.
 * @param queryObject The index of the query object.
 * @return A candidate list containing indices.
 */
template <typename T>
unordered_set<int> twoStepNeighbor(const SparseMatrix<T, RowMajor>& sparseMatrix,
                                   const SparseVector<T, RowMajor>& queryVector) {
    unordered_set<int> candidateList;
    unordered_set<int> queryCols = oneStepNeighbor<T>(queryVector);
    for (int i = 0; i < sparseMatrix.rows(); i++) {
        for (auto col: queryCols) {
            if (sparseMatrix.coeff(i, col) != 0) {
                candidateList.insert(i);
                break;
            }
        }
    }
    return candidateList;
}


/**
 * Calculates PathSim similarity only for all two-step neighbors.
 *
 * @param sparseMatrix The sparse matrix to operate on.
 * @param deltas The diagonal values/deltas that were previously calculated.
 * @param queryVector The query vector, i.e. the row of sparseMatrix corresponding to the query object.
 * @param queryDelta The diagonal value/delta of the query object.
 * @param k The number of most similar objects we want to retrieve.
 * @return The k most similar objects to the query object  with their respective similarity
 */
template <typename T>
set<pair<double, int>, greater<>> PathSimTopKQuery(const SparseMatrix<T, RowMajor>& sparseMatrix,
                                                   const vector<T>& deltas,
                                                   const SparseVector<T, RowMajor>& queryVector,
                                                   const T queryDelta,
                                                   const int k) {

    set<pair<double, int>, greater<>> result;
    unordered_set<int> candidateList = twoStepNeighbor<T>(sparseMatrix, queryVector);

    for (auto const &candidate: candidateList) {
        double similarity = 2.0 * queryVector.dot(sparseMatrix.row(candidate)) / (queryDelta + deltas[candidate]);
        result.insert(make_pair(similarity, candidate));
        if (result.size() > k) {
            result.erase(*result.rbegin());
        }
    }

    return result;
}



// ----------------------------------- Instantiation of template functions --------------------------


template unordered_set<int> oneStepNeighbor(const SparseVector<unsigned long long, RowMajor>&);


template unordered_set<int> oneStepNeighbor(const SparseVector<long double, RowMajor>&);


template unordered_set<int> twoStepNeighbor(const SparseMatrix<unsigned long long, RowMajor>&, 
                                            const SparseVector<unsigned long long, RowMajor>&);


template unordered_set<int> twoStepNeighbor(const SparseMatrix<long double, RowMajor>&, 
                                            const SparseVector<long double, RowMajor>&);


template set<pair<double, int>, greater<>> PathSimTopKQuery<unsigned long long>(const SparseMatrix<unsigned long long, RowMajor>&, 
                                                                                const vector<unsigned long long>&, 
                                                                                const SparseVector<unsigned long long, RowMajor>&, 
                                                                                const unsigned long long, 
                                                                                const int);


template set<pair<double, int>, greater<>> PathSimTopKQuery<long double>(const SparseMatrix<long double, RowMajor>&, 
                                                                         const vector<long double>&, 
                                                                         const SparseVector<long double, RowMajor>&, 
                                                                         const long double, 
                                                                         const int);
