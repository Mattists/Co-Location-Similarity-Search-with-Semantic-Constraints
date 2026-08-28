#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <system_error>
#include <vector>

#include <Eigen/Sparse>

#include "SpatialMetaPathCreator/SpatialMetaPathGrid.hpp"

namespace fs = std::filesystem;
using namespace Eigen;
using namespace std;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

int main(int argc, char *argv[]) {
    fs::path allDatasetPath = argv[1];

    vector<int> epsilonSizes = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 75000};
    constexpr int gridResolution = 9;
    constexpr int warmupRuns = 1;
    constexpr int measuredRuns = 10;

    for (int epsilonSize : epsilonSizes) {
        vector<long long> runtimes;

        for (int run = 0; run < warmupRuns + measuredRuns; run++) {
            bool isWarmup = run < warmupRuns;

            fs::path filePath = allDatasetPath / "YFCC" / "matrices" / ("posted#photo_GridRangeQueryWeighted" + std::to_string(epsilonSize) + "_" + std::to_string(gridResolution) + ".mtx");
            error_code ec;
            if (fs::exists(filePath)) {
                fs::remove(filePath, ec);
                if (ec) {
                    cerr << "Could not remove file for epsilon size " << epsilonSize << ": " << ec.message() << endl;
                }
            }

            fs::path binFilePath = allDatasetPath / "YFCC" / "matrices" / ("posted#photo_GridRangeQueryWeighted" + std::to_string(epsilonSize) + "_" + std::to_string(gridResolution) + ".mtx.bin");
            error_code binec;
            if (fs::exists(binFilePath)) {
                fs::remove(binFilePath, binec);
                if (binec) {
                    cerr << "Could not remove binary file for epsilon size " << epsilonSize << ": " << binec.message() << endl;
                }
            }

            auto t1 = high_resolution_clock::now();

            SparseMatrix<long double, RowMajor> sparseMatrix = loadGridCommutingMatrix<long double>(allDatasetPath.string(),
                                                                                                                  "YFCC",
                                                                                                                  "posted",
                                                                                                                  "photo",
                                                                                                                  "photosWGS84.csv",
                                                                                                                  epsilonSize,
                                                                                                                  true,
                                                                                                                  ',',
                                                                                                                  true,
                                                                                                                  true,
                                                                                                                  gridResolution);

            auto t2 = high_resolution_clock::now();

            volatile auto nnz = sparseMatrix.nonZeros();

            if (!isWarmup) {
                runtimes.push_back(duration_cast<milliseconds>(t2 - t1).count());
            }
        }

        double sum = accumulate(runtimes.begin(), runtimes.end(), 0.0);
        double mean = sum / runtimes.size();

        double sq_sum = 0.0;
        for (auto r : runtimes) {
            sq_sum += (r - mean) * (r - mean);
        }

        double stddev = sqrt(sq_sum / (runtimes.size() - 1));

        cout << "Epsilon Size: " << epsilonSize
             << ", Mean Runtime: " << mean << " ms"
             << ", StdDev: " << stddev << " ms" << endl;
    }
}
