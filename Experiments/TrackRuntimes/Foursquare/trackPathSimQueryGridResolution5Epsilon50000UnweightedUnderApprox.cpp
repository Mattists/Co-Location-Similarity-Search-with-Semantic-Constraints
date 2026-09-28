#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridUnderApprox", false, 25000, 5, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 50000});
}