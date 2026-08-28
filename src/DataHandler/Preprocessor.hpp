#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <string>
#include <vector>

template <typename T = unsigned long long>
void preprocessDataset(const std::string& datasetName,
                       const std::string& directory,
                       const bool nameByEdgeType = false,
                       const bool saveMatrixMarketFile = false,
                       const bool saveDeltaTextFile = false);

template <typename T = unsigned long long>
void generatePartialCommutingMatrix(const std::string& datasetName,
                                    const std::string& directory,
                                    const std::string& metaPath,
                                    const bool nameByEdgeType = false,
                                    const bool saveMatrixMarketFile = false,
                                    const bool saveDeltaTextFile = false);

template <typename T = unsigned long long>
void generatePartialCommutingMatrix(const std::string& datasetName,
                                    const std::string& directory,
                                    const std::vector<std::string>& metaPaths,
                                    const bool nameByEdgeType = false,
                                    const bool saveMatrixMarketFile = false,
                                    const bool saveDeltaTextFile = false);

#endif //PREPROCESSOR_HPP
