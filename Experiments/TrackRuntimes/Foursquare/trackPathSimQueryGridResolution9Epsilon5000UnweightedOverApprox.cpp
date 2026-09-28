#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 2722, 9, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 5000});
}