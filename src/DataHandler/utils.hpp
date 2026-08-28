#ifndef DATAHANDLER_UTILS_HPP
#define DATAHANDLER_UTILS_HPP

#include <Eigen/Sparse>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cstddef>


/**
 * Split a string into a vertexRow
 * @param s The string to split.
 * @param delimiter The delimiter to split the string by.
 * @return A vector containing the split values.
 */
inline std::vector<std::string> split(const std::string& s, const char delimiter) {
    std::vector<std::string> splits;
    std::string split;
    std::istringstream stringStream(s);

    while (std::getline(stringStream, split, delimiter)) {
        splits.push_back(split);
    }

    return splits;
}


/**
 * Serialize an vertexRow to string
 * @param vector The vector to serialize.
 * @return The serialized string.
 */
template <typename T = unsigned long long>
inline std::string serialize(const std::vector<T>& vector) {
    std::stringstream s;

    s << std::setprecision(std::numeric_limits<T>::max_digits10);

    for (std::size_t i = 0; i < vector.size(); i++) {
        s << vector[i];

        if (i + 1 < vector.size()) {
            s << ',';
        }
    }

    return s.str();
}


/**
 * Deserialize a vertexRow.
 * @param s The string to deserialize.
 * @return A vector containing the deserialized values.
 */
template <typename T>
std::vector<T> deserialize(const std::string& s);


/**
 * Deserialize a vertexRow.
 * @param s The string to deserialize.
 * @return A vector containing the deserialized values.
 */
template <>
inline std::vector<unsigned long long> deserialize(const std::string& s) {
    std::vector<std::string> result = split(s, ',');
    std::vector<unsigned long long> output;

    for (auto& i: result) {
        output.emplace_back(std::stoul(i));
    }

    return output;
}


/**
 * Deserialize a vertexRow.
 * @param s The string to deserialize.
 * @return A vector containing the deserialized values.
 */
template <>
inline std::vector<long double> deserialize(const std::string& s) {
    std::vector<std::string> result = split(s, ',');
    std::vector<long double> output;

    for (const auto& i : result) {
        output.emplace_back(std::stold(i));
    }

    return output;
}


/**
 * Calculates the diagonalVector for a given sparse Matrix.
 * 
 * @param sparseMatrix The sparse matrix where to compute the diagonalValues from.
 * @return A vector containing the diagonalValues of the sparse matrix.
 */
template <typename T = unsigned long long>
inline std::vector<T> diagonalMatrix(const Eigen::SparseMatrix<T, Eigen::RowMajor>& sparseMatrix) {
    std::vector<T> diagonalValues(static_cast<std::size_t>(sparseMatrix.rows()), T{});

    for (Eigen::Index row = 0; row < sparseMatrix.rows(); row++) {
        T diagonalValue {};
        for (typename Eigen::SparseMatrix<T, Eigen::RowMajor>::InnerIterator it(sparseMatrix, row); it; ++it) {
            diagonalValue += it.value() * it.value();
        }
        diagonalValues[static_cast<std::size_t>(row)] = diagonalValue;
    }

    return diagonalValues;
}



// ----------------------------------- Instantiation of template functions --------------------------

template std::string serialize(const std::vector<unsigned long long>& vector);

template std::string serialize(const std::vector<long double>& vector);

template std::vector<unsigned long long> diagonalMatrix(const Eigen::SparseMatrix<unsigned long long, Eigen::RowMajor>& sparseMatrix);

template std::vector<long double> diagonalMatrix(const Eigen::SparseMatrix<long double, Eigen::RowMajor>& sparseMatrix);


#endif //DATAHANDLER_UTILS_HPP
