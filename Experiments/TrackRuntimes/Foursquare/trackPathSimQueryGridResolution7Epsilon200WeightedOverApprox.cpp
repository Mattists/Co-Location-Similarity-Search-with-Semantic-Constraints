#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", true, 1649, 7, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 200});
}