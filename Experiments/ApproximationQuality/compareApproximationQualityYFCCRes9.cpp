#include "approximationQuality.hpp"

int main(int argc, char* argv[]) {
    return approximation_quality::runApproximationQuality(
            argc,
            argv,
            {"YFCC", "posted", "photo", "photosWGS84.csv"},
            9,
            222);
}
