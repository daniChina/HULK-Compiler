#include "diagnostic.hpp"

#include <algorithm>

#include "matcom_diagnostic.hpp"
#include "pipeline.hpp"

namespace hulk {

bool Diagnostic::operator<(const Diagnostic& other) const {
    if (line != other.line) {
        return line < other.line;
    }
    if (col != other.col) {
        return col < other.col;
    }
    return static_cast<int>(kind) < static_cast<int>(other.kind);
}

std::string format_diagnostic_item(const Diagnostic& item) {
    switch (item.kind) {
    case DiagnosticKind::Lexical:
        return format_lexical(item.line, item.col, item.message);
    case DiagnosticKind::Syntactic:
        return format_syntactic(item.line, item.col, item.message);
    case DiagnosticKind::Semantic:
        return format_semantic(item.line, item.col, item.message);
    }
    return item.message;
}

CompilePhase dominant_phase(const std::vector<Diagnostic>& items) {
    bool has_lexical = false;
    bool has_syntactic = false;
    bool has_semantic = false;

    for (const auto& item : items) {
        switch (item.kind) {
        case DiagnosticKind::Lexical:
            has_lexical = true;
            break;
        case DiagnosticKind::Syntactic:
            has_syntactic = true;
            break;
        case DiagnosticKind::Semantic:
            has_semantic = true;
            break;
        }
    }

    if (has_lexical) {
        return CompilePhase::Lexical;
    }
    if (has_syntactic) {
        return CompilePhase::Syntactic;
    }
    if (has_semantic) {
        return CompilePhase::Semantic;
    }
    return CompilePhase::Ok;
}

int exit_code_for_phase(CompilePhase phase) {
    switch (phase) {
    case CompilePhase::Lexical:
        return 1;
    case CompilePhase::Syntactic:
        return 2;
    case CompilePhase::Semantic:
        return 3;
    case CompilePhase::Ok:
        return 0;
    }
    return 3;
}

DiagnosticKind diagnostic_kind_for_phase(CompilePhase phase) {
    switch (phase) {
    case CompilePhase::Lexical:
        return DiagnosticKind::Lexical;
    case CompilePhase::Syntactic:
        return DiagnosticKind::Syntactic;
    case CompilePhase::Semantic:
        return DiagnosticKind::Semantic;
    case CompilePhase::Ok:
        return DiagnosticKind::Semantic;
    }
    return DiagnosticKind::Semantic;
}

std::vector<Diagnostic> deduplicate_diagnostics(std::vector<Diagnostic> items) {
    std::sort(items.begin(), items.end());
    std::vector<Diagnostic> unique;
    std::vector<std::string> seen;
    for (const auto& item : items) {
        const std::string formatted = format_diagnostic_item(item);
        if (std::find(seen.begin(), seen.end(), formatted) != seen.end()) {
            continue;
        }
        seen.push_back(formatted);
        unique.push_back(item);
    }
    return unique;
}

std::vector<Diagnostic> merge_diagnostics(std::vector<Diagnostic> first,
                                          std::vector<Diagnostic> second) {
    first.insert(first.end(), second.begin(), second.end());
    return deduplicate_diagnostics(std::move(first));
}

std::vector<Diagnostic> filter_diagnostics_by_mode(const std::vector<Diagnostic>& items,
                                                     bool all_errors) {
    if (all_errors || items.empty()) {
        return items;
    }

    const CompilePhase phase = dominant_phase(items);
    const DiagnosticKind keep = diagnostic_kind_for_phase(phase);
    std::vector<Diagnostic> filtered;
    for (const auto& item : items) {
        if (item.kind == keep) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

std::vector<std::string> format_diagnostic_lines(const std::vector<Diagnostic>& items) {
    std::vector<std::string> lines;
    lines.reserve(items.size());
    for (const auto& item : items) {
        lines.push_back(format_diagnostic_item(item));
    }
    return lines;
}

void finalize_compile_diagnostic(CompilePhase& phase,
                                 int& exit_code,
                                 std::vector<Diagnostic>& items,
                                 std::vector<std::string>& lines,
                                 bool all_errors) {
    items = deduplicate_diagnostics(std::move(items));
    const auto visible = filter_diagnostics_by_mode(items, all_errors);
    phase = dominant_phase(items);
    exit_code = exit_code_for_phase(phase);
    lines = format_diagnostic_lines(visible);
}

}  // namespace hulk
