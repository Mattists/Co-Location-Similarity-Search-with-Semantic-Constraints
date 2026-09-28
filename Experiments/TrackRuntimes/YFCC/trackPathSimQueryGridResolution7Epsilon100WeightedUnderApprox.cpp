#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridUnderApprox", true, 50, 7, "YFCC", "posted", "photo", "photosWGS84.csv", 100});
}