#include <iostream>
#include <filesystem>
#include <chrono>
#include <set>
#include <string>
#include <system_error>
#include <vector>
#include <utility>
#include <functional>

#include <Eigen/Sparse>

#include "DataHandler/MatrixIO.hpp"
#include "PathSim/PathSimQuery.hpp"
#include "SpatialMetaPathCreator/SpatialMetaPathGrid.hpp"

namespace fs = std::filesystem;
using namespace Eigen;
using namespace std;
using std::chrono::high_resolution_clock;
using std::chrono::time_point;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::seconds;

int main(int argc, char *argv[]) {

    int queryObject = 0;
    int k = 10;

    time_point<high_resolution_clock> t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12;

    fs::path allDatasetPath = argv[1];

    int epsilon = 6250;
    int gridResolution = 5;


    ////////////////////// Weighted Range Query //////////////////////

    {
        // remove matrix before generating new one to prevent simply loading it from disk
        fs::path filePath = allDatasetPath / "GeoDBLP" / "matrices" / ("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryWeighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution) + ".mtx");
        error_code ec;
        if (fs::exists(filePath)) {
            fs::remove(filePath, ec);
            if (ec) {
                cerr << "Could not remove file " << ec.message() << endl;
            }
        }
        // also remove bin matrix before generating new one to prevent simply loading it from disk
        fs::path binFilePath = allDatasetPath / "GeoDBLP" / "matrices" / ("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryWeighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution) + ".mtx.bin");
        error_code binec;
        if (fs::exists(binFilePath)) {
            fs::remove(binFilePath, binec);
            if (binec) {
                cerr << "Could not remove binary file: " << binec.message() << endl;
            }
        }
    }

    t5 = high_resolution_clock::now();
    SparseMatrix<long double, RowMajor> sparseMatrix2 = loadGridCommutingMatrix<long double>(allDatasetPath,
                                                                                             "GeoDBLP",
                                                                                             "published_in_conference^-1#wrote^-1#affiliated_with",
                                                                                             "affiliation",
                                                                                             "affiliationsWGS84.csv",
                                                                                             epsilon,
                                                                                             true,
                                                                                             ',',
                                                                                             true,
                                                                                             true,
                                                                                             gridResolution);
    t6 = high_resolution_clock::now();
    auto duration3 = duration_cast<milliseconds>(t6 - t5).count();
    cout << "Building weighted=true spatially extended commuting matrix complete after " << duration3 << "ms." << endl;

    t7 = high_resolution_clock::now();

    vector<long double> deltas2 = loadDeltas<long double>("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryWeighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution),
                                                          "GeoDBLP",
                                                          allDatasetPath);
    t8 = high_resolution_clock::now();
    auto duration4 = duration_cast<milliseconds>(t8 - t7).count();
    cout << "Getting weighted=true deltas complete after " << duration4 << "ms." << endl;

    set<pair<double, int>, greater<>> results2 = PathSimTopKQuery(sparseMatrix2,
                                                             deltas2,
                                                             SparseVector<long double, RowMajor>(sparseMatrix2.row(queryObject)),
                                                             deltas2[queryObject],
                                                             k);

    cout << "\nGRID-DISTANCE: Objects most similar to object " << queryObject << " using weighted=true:" << endl;

    for (const auto& result : results2) {
        cout << result.second << ": " << result.first << endl;
    }

    ////////////////////// Unweighted Range Query //////////////////////

    {
        // remove matrix before generating new one to prevent simply loading it from disk
        fs::path filePath = allDatasetPath / "GeoDBLP" / "matrices" / ("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryUnweighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution) + ".mtx");
        error_code ec;
        if (fs::exists(filePath)) {
            fs::remove(filePath, ec);
            if (ec) {
                cerr << "Could not remove file " << ec.message() << endl;
            }
        }
        // also remove bin matrix before generating new one to prevent simply loading it from disk
        fs::path binFilePath = allDatasetPath / "GeoDBLP" / "matrices" / ("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryUnweighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution) + ".mtx.bin");
        error_code binec;
        if (fs::exists(binFilePath)) {
            fs::remove(binFilePath, binec);
            if (binec) {
                cerr << "Could not remove binary file: " << binec.message() << endl;
            }
        }
    }

    t9 = high_resolution_clock::now();
    SparseMatrix<unsigned long long, RowMajor> sparseMatrix3 = loadGridCommutingMatrix<unsigned long long>(allDatasetPath,
                                                                                                           "GeoDBLP",
                                                                                                           "published_in_conference^-1#wrote^-1#affiliated_with",
                                                                                                           "affiliation",
                                                                                                           "affiliationsWGS84.csv",
                                                                                                           epsilon,
                                                                                                           false,
                                                                                                           ',',
                                                                                                           true,
                                                                                                           true,
                                                                                                           gridResolution);
    t10 = high_resolution_clock::now();
    auto duration5 = duration_cast<milliseconds>(t10 - t9).count();
    cout << "Building weighted=false spatially extended commuting matrix complete after " << duration5 << "ms." << endl;

    t11 = high_resolution_clock::now();

    vector<unsigned long long> deltas3 = loadDeltas("published_in_conference^-1#wrote^-1#affiliated_with#affiliation_GridRangeQueryUnweighted" + std::to_string(epsilon) + "_" + std::to_string(gridResolution),
                                                    "GeoDBLP",
                                                    allDatasetPath);
    t12 = high_resolution_clock::now();
    auto duration6 = duration_cast<milliseconds>(t12 - t11).count();
    cout << "Getting weighted=false deltas complete after " << duration6 << "ms." << endl;

    set<pair<double, int>, greater<>> results3 = PathSimTopKQuery(sparseMatrix3,
                                                             deltas3,
                                                             SparseVector<unsigned long long, RowMajor>(sparseMatrix3.row(queryObject)),
                                                             deltas3[queryObject],
                                                             k);

    cout << "\nGRID-DISTANCE: Objects most similar to object " << queryObject << " using weighted=false:" << endl;

    for (const auto& result : results3) {
        cout << result.second << ": " << result.first << endl;
    }
}


