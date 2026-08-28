#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"grid", true, 10000, 7, "YFCC", "posted", "photo", "photosWGS84.csv"});
}
