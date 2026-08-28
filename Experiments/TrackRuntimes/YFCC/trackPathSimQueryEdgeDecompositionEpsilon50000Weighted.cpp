#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"edgeDecomposition", true, 50000, track_runtimes::noGridResolution, "YFCC", "posted", "photo", "photosWGS84.csv"});
}
