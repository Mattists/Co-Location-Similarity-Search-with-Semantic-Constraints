#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", true, 1649, 7, "YFCC", "posted", "photo", "photosWGS84.csv", 200});
}