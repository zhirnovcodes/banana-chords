#include "Check.h"

int gFailures = 0;

int main() {
    sequencesTests();
    engineTests();
    chordsGeneratorTests();
    chordsLoadTests();
    if (gFailures == 0) {
        std::printf("bc_tests: all passed\n");
        return 0;
    }
    std::printf("bc_tests: %d failed\n", gFailures);
    return 1;
}
