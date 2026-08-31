// scout_validation — runs the synthetic validation battery and writes the
// markdown report (docs/contest/VALIDATION_REPORT.md is the committed
// snapshot). Exit code 0 only when every case behaves as specified, so the
// tool doubles as a quality gate.
#include "ValidationSuite.hpp"

#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "usage: scout_validation [REPORT.md]\n");
        return 2;
    }

    const auto run = platypus::validation::runValidation();
    const auto report = platypus::validation::formatReport(run);

    if (argc == 2) {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "error: cannot write %s\n", argv[1]);
            return 1;
        }
        out.write(report.data(), static_cast<std::streamsize>(report.size()));
        std::printf("wrote %s\n", argv[1]);
    } else {
        std::fputs(report.c_str(), stdout);
    }

    std::printf("%zu/%zu cases behaved as specified\n", run.summary.behaved, run.summary.cases);
    return run.summary.behaved == run.summary.cases ? 0 : 1;
}
