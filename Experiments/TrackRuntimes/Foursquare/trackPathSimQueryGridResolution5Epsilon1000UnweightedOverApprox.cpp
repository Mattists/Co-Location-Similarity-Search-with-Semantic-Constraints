#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 11338, 5, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 1000});
}