#pragma once

#include <string>
#include <vector>

namespace hulk {

enum class CompilePhase;

enum class DiagnosticKind {
    Lexical,
    Syntactic,
    Semantic
};

struct Diagnostic {
    int line = 0;
    int col = 0;
    DiagnosticKind kind = DiagnosticKind::Semantic;
    std::string message;

    bool operator<(const Diagnostic& other) const;
};

std::string format_diagnostic_item(const Diagnostic& item);

CompilePhase dominant_phase(const std::vector<Diagnostic>& items);
int exit_code_for_phase(CompilePhase phase);
DiagnosticKind diagnostic_kind_for_phase(CompilePhase phase);

std::vector<Diagnostic> merge_diagnostics(std::vector<Diagnostic> first,
                                          std::vector<Diagnostic> second);
std::vector<Diagnostic> deduplicate_diagnostics(std::vector<Diagnostic> items);
std::vector<Diagnostic> filter_diagnostics_by_mode(const std::vector<Diagnostic>& items,
                                                     bool all_errors);
std::vector<std::string> format_diagnostic_lines(const std::vector<Diagnostic>& items);

void finalize_compile_diagnostic(CompilePhase& phase,
                                 int& exit_code,
                                 std::vector<Diagnostic>& items,
                                 std::vector<std::string>& lines,
                                 bool all_errors);

}  // namespace hulk
