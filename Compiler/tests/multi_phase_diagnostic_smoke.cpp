#include <iostream>
#include <string>

#include "../pipeline.hpp"

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    std::cout << "[OK] " << message << "\n";
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    const std::string source =
        "print(a+2);\n"
        "print(b+3)\n";

    {
        hulk::CompileOptions options;
        options.all_errors = false;
        const auto compiled = hulk::compile_program(source, "Parser/grammar/grammar.ll1", options);
        ok &= expect(compiled.diagnostic.exit_code == 2, "first-phase mode stops at syntactic phase");
        ok &= expect(compiled.diagnostic.lines.size() == 1,
                     "first-phase mode reports one diagnostic line");
        ok &= expect(compiled.diagnostic.lines.front().find("SYNTACTIC:") != std::string::npos,
                     "first-phase mode line is syntactic");
    }

    {
        hulk::CompileOptions options;
        options.all_errors = true;
        const auto compiled = hulk::compile_program(source, "Parser/grammar/grammar.ll1", options);
        ok &= expect(compiled.diagnostic.exit_code == 2,
                     "all-errors keeps syntactic exit code when syntax present");
        ok &= expect(compiled.diagnostic.lines.size() == 2,
                     "all-errors reports semantic and syntactic");
        ok &= expect(compiled.diagnostic.lines.front().find("SEMANTIC:") != std::string::npos,
                     "all-errors orders semantic before syntactic");
        ok &= expect(compiled.diagnostic.lines.back().find("SYNTACTIC:") != std::string::npos,
                     "all-errors includes syntactic line");
        ok &= expect(compiled.program == nullptr, "all-errors does not keep program on failure");
    }

    return ok ? 0 : 1;
}
