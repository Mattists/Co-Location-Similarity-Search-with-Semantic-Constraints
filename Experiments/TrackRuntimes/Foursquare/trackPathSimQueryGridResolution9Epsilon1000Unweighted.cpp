#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"grid", false, 1000, 9, "Foursquare", "checked_in_at", "venue", "venuesWGS84.csv"});
}
