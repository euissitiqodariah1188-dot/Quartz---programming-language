#include "../include/formatter.h"
#include <sstream>
#include <fstream>
#include <cstring>

namespace quartz {

// ============================================================
//  QUARTZ SOURCE FORMATTER
//  Formats .qua files with consistent indentation and style
//  Rules:
//    - 4-space indentation
//    - Space around operators
//    - Space after comma
//    - Blank line between top-level declarations
//    - Consistent if/else/func/end! formatting
//    - Strip trailing whitespace
//    - Ensure single newline at end of file
// ============================================================

Formatter::Formatter(FormatterOptions opts) : opts_(std::move(opts)) {}

std::string Formatter::format_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        error_ = "Cannot open: " + path;
        return "";
    }
    std::string src((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    return format_source(src);
}

std::string Formatter::format_source(const std::string& src) {
    error_.clear();
    std::istringstream in(src);
    std::ostringstream out;

    std::string line;
    int indent = 0;
    bool last_was_blank = false;
    bool first_line = true;

    while (std::getline(in, line)) {
        // Strip trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' ||
               line.back() == '\r')) {
            line.pop_back();
        }

        // Skip multiple consecutive blank lines (keep max 1)
        if (line.empty()) {
            if (!last_was_blank && !first_line) {
                out << "\n";
                last_was_blank = true;
            }
            continue;
        }
        last_was_blank = false;
        first_line = false;

        // Strip leading whitespace to re-indent
        size_t start = line.find_first_not_of(" \t");
        std::string content = (start == std::string::npos) ? "" : line.substr(start);

        if (content.empty()) continue;

        // Determine indent adjustment BEFORE this line
        bool is_dedent = (content == "end!" ||
                          content.rfind("else", 0) == 0 ||
                          content.rfind("else:", 0) == 0);
        if (is_dedent && indent > 0) indent--;

        // Write with current indent
        std::string indented = make_indent(indent) + format_line(content);
        out << indented << "\n";

        // Determine indent adjustment AFTER this line
        bool opens_block = ends_with_colon(content) &&
                           !is_comment(content);
        if (opens_block) indent++;
    }

    std::string result = out.str();

    // Ensure single trailing newline
    while (result.size() >= 2 &&
           result[result.size()-1] == '\n' &&
           result[result.size()-2] == '\n') {
        result.pop_back();
    }
    if (result.empty() || result.back() != '\n') result += '\n';

    return result;
}

bool Formatter::format_file_inplace(const std::string& path) {
    std::string formatted = format_file(path);
    if (!error_.empty()) return false;
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) {
        error_ = "Cannot write: " + path;
        return false;
    }
    f << formatted;
    return true;
}

// ---- Internal helpers ----

std::string Formatter::make_indent(int level) const {
    return std::string(static_cast<size_t>(level * opts_.indent_size), ' ');
}

bool Formatter::ends_with_colon(const std::string& line) const {
    if (line.empty()) return false;
    // Check if line ends with ':' (but not inside a string)
    // Simple heuristic: last non-space char is ':'
    for (int i = static_cast<int>(line.size()) - 1; i >= 0; --i) {
        char c = line[static_cast<size_t>(i)];
        if (c == ':') return true;
        if (c != ' ' && c != '\t') return false;
    }
    return false;
}

bool Formatter::is_comment(const std::string& line) const {
    size_t s = line.find_first_not_of(" \t");
    if (s == std::string::npos) return false;
    return line.size() > s + 1 && line[s] == '/' && line[s+1] == '/';
}

std::string Formatter::format_line(const std::string& line) const {
    if (line.empty()) return line;
    if (is_comment(line)) return line;

    std::string result;
    result.reserve(line.size() + 8);

    bool in_string = false;
    char string_char = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        // Track string state
        if (!in_string && (c == '"' || c == '\'')) {
            in_string = true;
            string_char = c;
            result += c;
            continue;
        }
        if (in_string && c == string_char && (i == 0 || line[i-1] != '\\')) {
            in_string = false;
            result += c;
            continue;
        }
        if (in_string) {
            result += c;
            continue;
        }

        // Space around = (assignment), but not ==, !=, <=, >=
        if (c == '=' && i + 1 < line.size()) {
            char next = line[i+1];
            char prev = result.empty() ? 0 : result.back();
            if (next != '=' && prev != '!' && prev != '<' && prev != '>' && prev != '=') {
                // Assignment =
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += '=';
                if (i + 1 < line.size() && line[i+1] != ' ') result += ' ';
                continue;
            }
        }

        // Space after comma
        if (c == ',' && opts_.space_after_comma) {
            result += ',';
            if (i + 1 < line.size() && line[i+1] != ' ') result += ' ';
            continue;
        }

        result += c;
    }

    // Strip trailing spaces from formatted line
    while (!result.empty() && result.back() == ' ') result.pop_back();

    return result;
}

} // namespace quartz
