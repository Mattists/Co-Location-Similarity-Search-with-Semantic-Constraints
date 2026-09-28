#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 26549, 7, "YFCC", "posted", "photo", "photosWGS84.csv", 50000});
}