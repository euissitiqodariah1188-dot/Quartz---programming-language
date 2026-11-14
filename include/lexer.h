#pragma once
#ifndef QUARTZ_LEXER_H
#define QUARTZ_LEXER_H

#include "quartz.h"
#include <string_view>

namespace quartz {

// ============================================================
//  QUARTZ LEXER
//  Converts source code → token stream
//  Performance target: < 0.001ms for typical files
// ============================================================
class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<source>");
    ~Lexer() = default;

    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;

    // Tokenize entire source → returns all tokens
    std::vector<Token> tokenize();

    // Get one token at a time
    Token next_token();
    Token peek_token();

    // State
    bool   at_end()    const noexcept { return pos_ >= src_.size(); }
    size_t position()  const noexcept { return pos_; }
    uint32_t line()    const noexcept { return line_; }
    uint32_t column()  const noexcept { return col_; }

    // Error reporting
    std::string last_error() const { return error_; }
    bool has_error()         const { return !error_.empty(); }

private:
    std::string src_;
    std::string filename_;
    size_t      pos_   = 0;
    uint32_t    line_  = 1;
    uint32_t    col_   = 1;
    std::string error_;

    // Indent tracking for Python-like blocks
    std::vector<int> indent_stack_;
    int              current_indent_ = 0;
    bool             at_line_start_  = true;

    // Internal tokenizer methods
    QZ_INLINE char   current()     const noexcept;
    QZ_INLINE char   lookahead(int offset = 1) const noexcept;
    QZ_INLINE char   advance()     noexcept;
    QZ_INLINE void   skip_whitespace_no_newline();
    QZ_INLINE void   skip_comment();

    Token read_number();
    Token read_string(char quote);
    Token read_identifier_or_keyword();
    Token read_operator();
    Token read_newline_and_indent();

    // Keyword lookup
    static TokenType lookup_keyword(const std::string& word) noexcept;

    // Error token
    Token error_token(const std::string& msg);

    // Build token helper
    QZ_INLINE Token make_token(TokenType t, std::string val) const noexcept;
};

} // namespace quartz

#endif // QUARTZ_LEXER_H
