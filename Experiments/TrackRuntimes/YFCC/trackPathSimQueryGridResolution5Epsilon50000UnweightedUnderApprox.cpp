#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridUnderApprox", false, 25000, 5, "YFCC", "posted", "photo", "photosWGS84.csv", 50000});
}