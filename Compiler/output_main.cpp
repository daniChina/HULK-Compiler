#include "pipeline.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string read_embedded_program() {
    std::ifstream input(".hulk_program.hulk");
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

int main() {
    const std::string source = read_embedded_program();
    if (source.empty()) {
        std::cerr << "missing embedded program (.hulk_program.hulk)\n";
        return 1;
    }
    return hulk::run_embedded_program(source, std::cout, std::cerr);
}
