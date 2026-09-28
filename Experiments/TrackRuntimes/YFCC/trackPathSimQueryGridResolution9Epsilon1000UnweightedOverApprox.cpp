#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 722, 9, "YFCC", "posted", "photo", "photosWGS84.csv", 1000});
}