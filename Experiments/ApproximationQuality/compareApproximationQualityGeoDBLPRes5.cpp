#include "approximationQuality.hpp"

int main(int argc, char* argv[]) {
    return approximation_quality::runApproximationQuality(
            argc,
            argv,
            {"GeoDBLP", "published_in_conference^-1#wrote^-1#affiliated_with", "affiliation", "affiliationsWGS84.csv"},
            5,
            10838);
}
