#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"edgeDecomposition", false, 500, track_runtimes::noGridResolution, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv"});
}
