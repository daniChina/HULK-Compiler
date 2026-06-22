#include <iostream>
#include <string>
#include <vector>

#include "../diagnostic.hpp"
#include "../pipeline.hpp"

namespace {

using hulk::CompilePhase;
using hulk::Diagnostic;
using hulk::DiagnosticKind;

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

    {
        const std::vector<Diagnostic> first = {
            {2, 10, DiagnosticKind::Syntactic, "missing semicolon"},
            {1, 7, DiagnosticKind::Semantic, "undefined a"},
        };
        const std::vector<Diagnostic> second = {
            {1, 7, DiagnosticKind::Semantic, "undefined a"},
        };
        const auto merged = hulk::merge_diagnostics(first, second);
        ok &= expect(merged.size() == 2, "merge deduplicates identical diagnostics");
        ok &= expect(merged.front().line == 1 && merged.front().kind == DiagnosticKind::Semantic,
                     "merge sorts by line,col");
        ok &= expect(merged.back().line == 2 && merged.back().kind == DiagnosticKind::Syntactic,
                     "merge keeps later line after sort");
    }

    {
        const std::vector<Diagnostic> items = {
            {1, 1, DiagnosticKind::Semantic, "sem"},
            {2, 1, DiagnosticKind::Syntactic, "syn"},
        };
        ok &= expect(hulk::dominant_phase(items) == CompilePhase::Syntactic,
                     "dominant phase prefers syntactic over semantic");
        const auto filtered = hulk::filter_diagnostics_by_mode(items, false);
        ok &= expect(filtered.size() == 1 && filtered.front().kind == DiagnosticKind::Syntactic,
                     "first-phase mode keeps only dominant phase");
        const auto all = hulk::filter_diagnostics_by_mode(items, true);
        ok &= expect(all.size() == 2, "all-errors (default) mode keeps every phase");
    }

    {
        CompilePhase phase = CompilePhase::Ok;
        int exit_code = 0;
        std::vector<Diagnostic> items = {
            {1, 1, DiagnosticKind::Semantic, "sem"},
            {2, 1, DiagnosticKind::Syntactic, "syn"},
        };
        std::vector<std::string> lines;
        hulk::finalize_compile_diagnostic(phase, exit_code, items, lines, false);
        ok &= expect(exit_code == 2, "finalize exit code follows dominant syntactic phase");
        ok &= expect(lines.size() == 1, "finalize first-phase mode exposes one line");
        ok &= expect(lines.front().find("SYNTACTIC:") != std::string::npos,
                     "finalize formats syntactic line");
    }

    return ok ? 0 : 1;
}
