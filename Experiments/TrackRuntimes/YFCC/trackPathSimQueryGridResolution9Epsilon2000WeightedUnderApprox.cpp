#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridUnderApprox", true, 1000, 9, "YFCC", "posted", "photo", "photosWGS84.csv", 2000});
}