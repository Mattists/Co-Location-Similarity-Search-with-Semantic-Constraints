#include "MatrixIO.hpp"

#include "utils.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>

using namespace Eigen;
using namespace std;
namespace fs = std::filesystem;

/**
 * Returns the path to the binary file for the given matrix or delta path.
 * 
 * @param matrixPath The path to the original file.
 * @return The path to the binary file.
 */
static fs::path getBinaryPath(const fs::path& path) {
    fs::path binaryPath = path;
    binaryPath += ".bin";
    return binaryPath;
}


/** 
 * Writes a binary value to the given stream.
 * 
 * @param stream The output stream.
 * @param value The value to write.
 */
template <typename T>
static void writeBinaryValue(ofstream& stream,
                             const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}


/**
 * Reads a binary value from the given stream.
 * 
 * @param stream The input stream.
 * @param value The value to read.
 * @return True if the value was read successfully, false otherwise.
 */
template <typename T>
static bool readBinaryValue(ifstream& stream,
                            T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

static bool binaryCacheIsCurrent(const fs::path& matrixPath,
                                 const fs::path& binaryPath) {
    if (!fs::exists(binaryPath)) {
        return false;
    }

    if (!fs::exists(matrixPath)) {
        return true;
    }

    return fs::last_write_time(binaryPath) >= fs::last_write_time(matrixPath);
}


/** 
 * Saves a sparse matrix to a binary file.
 * 
 * @param sparseMatrix The sparse matrix to save.
 * @param matrixPath The path to the binary file.
 */
template <typename T>
void saveMatrixBinary(const SparseMatrix<T, RowMajor>& sparseMatrix,
                      const fs::path& matrixPath) {
    using MatrixType = SparseMatrix<T, RowMajor>;
    using StorageIndex = typename MatrixType::StorageIndex;

    fs::create_directories(matrixPath.parent_path());
    const fs::path binaryPath = getBinaryPath(matrixPath);

    MatrixType compressedCopy;
    const MatrixType* matrixToWrite = &sparseMatrix;
    if (!sparseMatrix.isCompressed()) {
        compressedCopy = sparseMatrix;
        compressedCopy.makeCompressed();
        matrixToWrite = &compressedCopy;
    }

    ofstream stream(binaryPath, ios::binary | ios::trunc);
    if (!stream.is_open()) {
        throw runtime_error("Could not open binary matrix cache for writing: " + binaryPath.string());
    }

    constexpr array<char, 8> magic = {'P', 'P', 'S', 'M', 'T', 'X', '1', '\0'};
    constexpr uint64_t version = 1;
    const uint64_t rows = static_cast<uint64_t>(matrixToWrite->rows());
    const uint64_t cols = static_cast<uint64_t>(matrixToWrite->cols());
    const uint64_t nonZeros = static_cast<uint64_t>(matrixToWrite->nonZeros());
    const uint64_t outerSize = static_cast<uint64_t>(matrixToWrite->outerSize());
    const uint64_t scalarSize = sizeof(T);
    const uint64_t storageIndexSize = sizeof(StorageIndex);
    const uint64_t rowMajor = MatrixType::IsRowMajor ? 1 : 0;

    stream.write(magic.data(), magic.size());
    writeBinaryValue(stream, version);
    writeBinaryValue(stream, rows);
    writeBinaryValue(stream, cols);
    writeBinaryValue(stream, nonZeros);
    writeBinaryValue(stream, outerSize);
    writeBinaryValue(stream, scalarSize);
    writeBinaryValue(stream, storageIndexSize);
    writeBinaryValue(stream, rowMajor);

    stream.write(reinterpret_cast<const char*>(matrixToWrite->outerIndexPtr()),
                 static_cast<streamsize>((outerSize + 1) * sizeof(StorageIndex)));
    stream.write(reinterpret_cast<const char*>(matrixToWrite->innerIndexPtr()),
                 static_cast<streamsize>(nonZeros * sizeof(StorageIndex)));
    stream.write(reinterpret_cast<const char*>(matrixToWrite->valuePtr()),
                 static_cast<streamsize>(nonZeros * sizeof(T)));

    if (!stream) {
        throw runtime_error("Could not write binary matrix cache: " + binaryPath.string());
    }
}


/**
 * Loads a sparse matrix from a binary file.
 * 
 * @param matrixPath The path to the binary file.
 * @param sparseMatrix The sparse matrix to load.
 * @return True if the matrix was loaded successfully, false otherwise.
 */
template <typename T>
bool loadMatrixBinary(const fs::path& matrixPath,
                      SparseMatrix<T, RowMajor>& sparseMatrix) {
    using MatrixType = SparseMatrix<T, RowMajor>;
    using StorageIndex = typename MatrixType::StorageIndex;
    using MatrixIndex = Eigen::Index;

    const fs::path binaryPath = getBinaryPath(matrixPath);
    if (!binaryCacheIsCurrent(matrixPath, binaryPath)) {
        return false;
    }

    ifstream stream(binaryPath, ios::binary);
    if (!stream.is_open()) {
        return false;
    }

    constexpr array<char, 8> expectedMagic = {'P', 'P', 'S', 'M', 'T', 'X', '1', '\0'};
    array<char, 8> magic {};
    uint64_t version = 0;
    uint64_t rows = 0;
    uint64_t cols = 0;
    uint64_t nonZeros = 0;
    uint64_t outerSize = 0;
    uint64_t scalarSize = 0;
    uint64_t storageIndexSize = 0;
    uint64_t rowMajor = 0;

    stream.read(magic.data(), magic.size());
    if (!stream || magic != expectedMagic ||
        !readBinaryValue(stream, version) ||
        !readBinaryValue(stream, rows) ||
        !readBinaryValue(stream, cols) ||
        !readBinaryValue(stream, nonZeros) ||
        !readBinaryValue(stream, outerSize) ||
        !readBinaryValue(stream, scalarSize) ||
        !readBinaryValue(stream, storageIndexSize) ||
        !readBinaryValue(stream, rowMajor)) {
        return false;
    }

    if (version != 1 ||
        scalarSize != sizeof(T) ||
        storageIndexSize != sizeof(StorageIndex) ||
        rowMajor != (MatrixType::IsRowMajor ? 1u : 0u) ||
        rows > static_cast<uint64_t>(numeric_limits<MatrixIndex>::max()) ||
        cols > static_cast<uint64_t>(numeric_limits<MatrixIndex>::max()) ||
        nonZeros > static_cast<uint64_t>(numeric_limits<MatrixIndex>::max()) ||
        outerSize != rows) {
        return false;
    }

    vector<StorageIndex> outerIndices(static_cast<size_t>(outerSize + 1));
    vector<StorageIndex> innerIndices(static_cast<size_t>(nonZeros));
    vector<T> values(static_cast<size_t>(nonZeros));

    stream.read(reinterpret_cast<char*>(outerIndices.data()),
                static_cast<streamsize>(outerIndices.size() * sizeof(StorageIndex)));
    stream.read(reinterpret_cast<char*>(innerIndices.data()),
                static_cast<streamsize>(innerIndices.size() * sizeof(StorageIndex)));
    stream.read(reinterpret_cast<char*>(values.data()),
                static_cast<streamsize>(values.size() * sizeof(T)));
    if (!stream) {
        return false;
    }

    Map<const MatrixType> mappedMatrix(static_cast<MatrixIndex>(rows),
                                       static_cast<MatrixIndex>(cols),
                                       static_cast<MatrixIndex>(nonZeros),
                                       outerIndices.data(),
                                       innerIndices.data(),
                                       values.data());
    sparseMatrix = mappedMatrix;
    sparseMatrix.makeCompressed();
    return true;
}

/**
 * Loads an adjacency matrix. 
 * Stores the binary matrix if only the market matrix was stored previously.
 *
 * @param matrixPath The path leading to the matrix file.
 * @param sparseMatrix Variable to store the matrix.
 * @return A Boolean indicating whether the matrix was loaded successfully.
 */
template <typename T>
bool loadMatrix(const fs::path& matrixPath,
                SparseMatrix<T, RowMajor>& sparseMatrix) {
    if (loadMatrixBinary<T>(matrixPath, sparseMatrix)) {
        return true;
    }

    ifstream matrixStream(matrixPath);
    if (matrixStream.is_open()) {
        matrixStream.close();
        loadMarket(sparseMatrix, matrixPath);
        sparseMatrix.makeCompressed();
        saveMatrixBinary<T>(sparseMatrix, matrixPath);
        return true;
    }
    return false;
}


/** 
 * Saves a delta vector to a binary file.
 * 
 * @param deltas The delta vector to save.
 * @param deltaPath The path to the binary file.
 */
template <typename T>
void saveDeltasBinary(const vector<T>& deltas,
                      const fs::path& deltaPath) {
    fs::create_directories(deltaPath.parent_path());
    const fs::path binaryPath = getBinaryPath(deltaPath);

    ofstream stream(binaryPath, ios::binary | ios::trunc);
    if (!stream.is_open()) {
        throw runtime_error("Could not open binary delta cache for writing: " + binaryPath.string());
    }

    constexpr array<char, 8> magic = {'P', 'P', 'S', 'D', 'L', 'T', '1', '\0'};
    constexpr uint64_t version = 1;
    const uint64_t valueCount = static_cast<uint64_t>(deltas.size());
    const uint64_t scalarSize = sizeof(T);

    stream.write(magic.data(), magic.size());
    writeBinaryValue(stream, version);
    writeBinaryValue(stream, valueCount);
    writeBinaryValue(stream, scalarSize);

    if (!deltas.empty()) {
        stream.write(reinterpret_cast<const char*>(deltas.data()),
                     static_cast<streamsize>(deltas.size() * sizeof(T)));
    }

    if (!stream) {
        throw runtime_error("Could not write binary delta cache: " + binaryPath.string());
    }
}


/**
 * Loads a delta vector from a binary file.
 * 
 * @param deltaPath The path to the binary file.
 * @param deltas The vector to store the loaded deltas.
 * @return True if the deltas were loaded successfully, false otherwise.
 */
template <typename T>
bool loadDeltasBinary(const fs::path& deltaPath,
                      vector<T>& deltas) {
    const fs::path binaryPath = getBinaryPath(deltaPath);
    if (!binaryCacheIsCurrent(deltaPath, binaryPath)) {
        return false;
    }

    ifstream stream(binaryPath, ios::binary);
    if (!stream.is_open()) {
        return false;
    }

    constexpr array<char, 8> expectedMagic = {'P', 'P', 'S', 'D', 'L', 'T', '1', '\0'};
    array<char, 8> magic {};
    uint64_t version = 0;
    uint64_t valueCount = 0;
    uint64_t scalarSize = 0;

    stream.read(magic.data(), magic.size());
    if (!stream || magic != expectedMagic ||
        !readBinaryValue(stream, version) ||
        !readBinaryValue(stream, valueCount) ||
        !readBinaryValue(stream, scalarSize)) {
        return false;
    }

    if (version != 1 ||
        scalarSize != sizeof(T) ||
        valueCount > static_cast<uint64_t>(numeric_limits<size_t>::max())) {
        return false;
    }

    vector<T> loadedDeltas(static_cast<size_t>(valueCount));
    if (!loadedDeltas.empty()) {
        stream.read(reinterpret_cast<char*>(loadedDeltas.data()),
                    static_cast<streamsize>(loadedDeltas.size() * sizeof(T)));
        if (!stream) {
            return false;
        }
    }

    deltas = move(loadedDeltas);
    return true;
}


/**
 * Creates a delta vector from the diagonal of a commuting matrix and saves it to a binary file.
 * Optionally saves the delta vector to a text file as well.
 * 
 * @param sparseMatrix The sparse matrix from which to create the delta vector.
 * @param matrixName The name of the left half commuting matrix/meta path for which to create the delta vector. 
 *                   Kept as is if shortName is chosen due to a limitation in file name length, otherwise it is mirrored and duplicated.
 *                   Has to be completed by the matrixNameAddition in the end to obtain the complete file name.
 * @param directory The directory in which to save the delta vector.
 * @param nameByEdgeType Whether to name the delta vector by edge type.
 * @param matrixNameAddition An optional addition to the matrix name for the delta vector.
 * @param shortName Whether to use a short name (omit the "way back" of the round trip meta path) due to the digit limit in operating systems
 * @param saveDeltaTextFile Whether to save the delta vector to a non-binary file as well.
 */
template <typename T>
void createAndStoreDeltas(const SparseMatrix<T, RowMajor>& sparseMatrix,
                          const string& matrixName,
                          const string& directory,
                          const bool nameByEdgeType,
                          const string& matrixNameAddition,
                          const bool shortName,
                          const bool saveDeltaTextFile) {

    vector<T> diagonalValues = diagonalMatrix<T>(sparseMatrix);

    fs::path deltaPath;
    if (shortName) {
        deltaPath = fs::path(directory) / (matrixName + matrixNameAddition + ".delta");
    } else {
        string secondHalf = matrixName;
        if (nameByEdgeType) {
            // '#' is assumed to be the character to separate different edges in a meta path when nameByEdgeType is true 
            // since '_' is used in naming  for some datasets, e.g., "find_site" and the '-' is used to denote the reverse edge "^-1"
            secondHalf = "#(" + secondHalf + ")^-1";
        } else {
            secondHalf.pop_back();
            reverse(secondHalf.begin(), secondHalf.end());
        }
        deltaPath = fs::path(directory) / (matrixName + secondHalf + matrixNameAddition + ".delta");
    }

    saveDeltasBinary<T>(diagonalValues, deltaPath);

    if (saveDeltaTextFile) {
        ofstream csvFile(deltaPath);
        csvFile << serialize(diagonalValues);
        csvFile.close();
    }
}


/**
 * Loads a delta vector.
 * Stores the binary delta vector if only the conventional vector was stored previously.
 *
 * @param matrixName The name of the commuting matrix/meta path for which to load deltas.
 * @param datasetName The name of the dataset containing the delta file.
 * @param directory The directory containing the delta file.
 * @return The delta vector.
 */
template <typename T>
vector<T> loadDeltas(const string& matrixName,
                     const string& datasetName,
                     const string& directory) {
    fs::path deltaPath = fs::path(directory) / datasetName / "matrices" / (matrixName + ".delta");
    vector<T> result;
    if (loadDeltasBinary<T>(deltaPath, result)) {
        return result;
    }

    ifstream csvFile(deltaPath);
    if (!csvFile.is_open()) {
        throw runtime_error("Could not open delta file for matrix " + matrixName + " in dataset "
                            + datasetName + " at " + deltaPath.string() + " or "
                            + getBinaryPath(deltaPath).string() + ". "
                            + "Did you use the short option when creating the delta file? "
                            + "If so only use the first half of matrixName (e.g., APV for APVPA) as input.");
    }

    string line;
    getline(csvFile, line);
    result = deserialize<T>(line);
    csvFile.close();
    saveDeltasBinary<T>(result, deltaPath);
    return result;
}

template bool loadMatrix(const fs::path&,
                         SparseMatrix<unsigned long long, RowMajor>&);

template bool loadMatrix(const fs::path&,
                         SparseMatrix<long double, RowMajor>&);

template void saveMatrixBinary(const SparseMatrix<unsigned long long, RowMajor>&,
                               const fs::path&);

template void saveMatrixBinary(const SparseMatrix<long double, RowMajor>&,
                               const fs::path&);

template bool loadMatrixBinary(const fs::path&,
                               SparseMatrix<unsigned long long, RowMajor>&);

template bool loadMatrixBinary(const fs::path&,
                               SparseMatrix<long double, RowMajor>&);

template void saveDeltasBinary(const vector<unsigned long long>&,
                               const fs::path&);

template void saveDeltasBinary(const vector<long double>&,
                               const fs::path&);

template bool loadDeltasBinary(const fs::path&,
                               vector<unsigned long long>&);

template bool loadDeltasBinary(const fs::path&,
                               vector<long double>&);

template void createAndStoreDeltas(const SparseMatrix<unsigned long long, RowMajor>&,
                                   const string&,
                                   const string&,
                                   const bool,
                                   const string&,
                                   const bool,
                                   const bool);

template void createAndStoreDeltas(const SparseMatrix<long double, RowMajor>&,
                                   const string&,
                                   const string&,
                                   const bool,
                                   const string&,
                                   const bool,
                                   const bool);

template vector<unsigned long long> loadDeltas<unsigned long long>(const string&,
                                                                   const string&,
                                                                   const string&);

template vector<long double> loadDeltas<long double>(const string&,
                                                     const string&,
                                                     const string&);
