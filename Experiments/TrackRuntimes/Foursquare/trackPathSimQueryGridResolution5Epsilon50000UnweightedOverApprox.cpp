#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", false, 35838, 5, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 50000});
}