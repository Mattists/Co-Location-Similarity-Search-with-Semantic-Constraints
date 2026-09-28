#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 13338, 5, "YFCC", "posted", "photo", "photosWGS84.csv", 5000});
}