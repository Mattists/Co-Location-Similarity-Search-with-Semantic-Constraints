#include "approximationQuality.hpp"

int main(int argc, char* argv[]) {
    return approximation_quality::runApproximationQuality(
            argc,
            argv,
            {"Foursquare", "checked_in_at", "venue", "venuesWGS84.csv"},
            8,
            586);
}
