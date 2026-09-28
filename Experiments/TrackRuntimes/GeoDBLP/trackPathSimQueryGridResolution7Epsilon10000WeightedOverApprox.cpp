#include "../trackPathSimQueryRuntimes.hpp"

int main(int argc, char* argv[]) {
    return track_runtimes::runPathSimQueryRuntimeTracking(
            argc,
            argv,
            {"gridOverApprox", true, 6549, 7, "GeoDBLP", "published_in_conference^-1#wrote^-1#affiliated_with", "affiliation", "affiliationsWGS84.csv", 10000});
}