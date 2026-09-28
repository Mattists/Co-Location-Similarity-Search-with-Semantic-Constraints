#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridUnderApprox", true, 25000, 9, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv", 50000});
}