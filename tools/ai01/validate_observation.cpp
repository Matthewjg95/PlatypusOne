// Host interoperability gate: validate Python output with the canonical C++ contract.
#include <platypus/observation/Observation.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    const auto decoded = platypus::observation::fromJson(text);
    if (!decoded.ok()) {
        std::cerr << decoded.error << '\n';
        return 1;
    }
    const auto errors = platypus::observation::validate(*decoded.record);
    for (const auto& error : errors)
        std::cerr << error << '\n';
    return errors.empty() ? 0 : 1;
}
