#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"grid", false, 5000, 9, "YFCC", "posted", "photo", "photosWGS84.csv"});
}
