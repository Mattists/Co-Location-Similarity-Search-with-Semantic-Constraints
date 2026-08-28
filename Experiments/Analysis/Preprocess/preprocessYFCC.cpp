#include <iostream>
#include <filesystem>
#include <chrono>
#include <map>
#include <vector>
#include <string>

#include "DataHandler/Preprocessor.hpp"


namespace fs = std::filesystem;
using namespace std;
using std::chrono::high_resolution_clock;
using std::chrono::time_point;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using std::chrono::seconds;


int main(int argc, char *argv[]) {

    time_point<high_resolution_clock> t1, t2;

    if (argc != 2) {
        cout << "Usage: ./preprocessYFCC <path_to_all_datasets>" << endl;
        return 1;
    }

    map<string, vector<string>> metaPathsPerDataset = {
            {"YFCC",       {"posted"}}
    };

    // preprocess all datasets in the directory specified by the first command line argument
    fs::path allDatasetPath = argv[1];
    for (const auto &entry : fs::directory_iterator(allDatasetPath)) {
        string datasetName = entry.path().filename();
        cout << "Preprocessing " << datasetName << "..." << endl;
        t1 = high_resolution_clock::now();
        if (datasetName == "YFCC") {
            preprocessDataset(datasetName, allDatasetPath, true);
        }
        t2 = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(t2 - t1).count();
        cout << "... preprocessing of " << datasetName << " complete after " << duration << "ms." << endl;
    }

    // generate the partial commuting matrices for specified datasets
    for (auto &[dataset, metaPaths]: metaPathsPerDataset) {
        if (dataset == "YFCC") {
            generatePartialCommutingMatrix<unsigned long long>(dataset, allDatasetPath, metaPaths, true);
        }
    }
}
