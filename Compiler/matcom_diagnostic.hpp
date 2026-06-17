#pragma once

#include <sstream>
#include <string>

namespace hulk {

inline std::string format_diagnostic(int line, int col, const std::string& type,
                                     const std::string& message) {
    std::ostringstream out;
    out << '(' << line << ',' << col << ") " << type << ": " << message;
    return out.str();
}

inline std::string format_lexical(int line, int col, const std::string& message) {
    return format_diagnostic(line, col, "LEXICAL", message);
}

inline std::string format_syntactic(int line, int col, const std::string& message) {
    return format_diagnostic(line, col, "SYNTACTIC", message);
}

inline std::string format_semantic(int line, int col, const std::string& message) {
    return format_diagnostic(line, col, "SEMANTIC", message);
}

}  // namespace hulk
