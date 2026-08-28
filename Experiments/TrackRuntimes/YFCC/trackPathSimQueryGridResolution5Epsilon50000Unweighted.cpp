#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"grid", false, 50000, 5, "YFCC", "posted", "photo", "photosWGS84.csv"});
}
