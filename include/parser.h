#pragma once
#ifndef QUARTZ_PARSER_H
#define QUARTZ_PARSER_H

#include "quartz.h"
#include "lexer.h"
#include "ast.h"

namespace quartz {

// ============================================================
//  QUARTZ PARSER
//  Token stream → AST
//  Handles all Quartz syntax
// ============================================================
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ~Parser() = default;

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    // Parse full program
    std::unique_ptr<ast::ProgramNode> parse();

    // Error state
    bool        has_error() const { return !errors_.empty(); }
    std::string error_summary() const;
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::vector<Token>  tokens_;
    size_t              pos_      = 0;
    std::vector<std::string> errors_;

    // --- Token stream helpers ---
    QZ_INLINE const Token& current()  const noexcept;
    QZ_INLINE const Token& peek(int offset = 1) const noexcept;
    QZ_INLINE const Token& advance()  noexcept;
    QZ_INLINE bool  at_end()         const noexcept;
    QZ_INLINE bool  check(TokenType t) const noexcept;
    QZ_INLINE bool  match(TokenType t) noexcept;
    Token           consume(TokenType t, const char* msg);
    void            skip_newlines()    noexcept;

    // --- Top-level parsers ---
    ast::NodePtr parse_statement();
    ast::NodePtr parse_main_decl();
    ast::NodePtr parse_func_decl();
    ast::NodePtr parse_var_decl();
    ast::NodePtr parse_import();
    ast::NodePtr parse_using();

    // --- Statement parsers ---
    ast::NodePtr parse_print_stmt();
    ast::NodePtr parse_input_stmt();
    ast::NodePtr parse_split_stmt();
    ast::NodePtr parse_point_stmt();
    ast::NodePtr parse_save_stmt();
    ast::NodePtr parse_mov_stmt();
    ast::NodePtr parse_add_stmt();
    ast::NodePtr parse_wait_stmt();
    ast::NodePtr parse_util_stmt();
    ast::NodePtr parse_gpu_render_stmt();
    ast::NodePtr parse_return_stmt();
    ast::NodePtr parse_if_stmt();
    ast::NodePtr parse_when_stmt();
    ast::NodePtr parse_from_stmt();
    ast::NodePtr parse_asm_stmt();
    ast::NodePtr parse_mem_stmt();
    ast::NodePtr parse_hw_stmt();
    ast::NodePtr parse_gpu_stmt();
    ast::NodePtr parse_expr_stmt();

    // --- Expression parsers ---
    ast::NodePtr parse_expr();
    ast::NodePtr parse_or_expr();
    ast::NodePtr parse_and_expr();
    ast::NodePtr parse_equality_expr();
    ast::NodePtr parse_relational_expr();
    ast::NodePtr parse_additive_expr();
    ast::NodePtr parse_multiplicative_expr();
    ast::NodePtr parse_unary_expr();
    ast::NodePtr parse_primary_expr();
    ast::NodePtr parse_call_expr(std::string callee);
    ast::NodePtr parse_math_func(TokenType op);
    ast::NodePtr parse_array_literal();

    // --- Block parser ---
    ast::NodeList parse_block();

    // --- Error handling ---
    void   emit_error(const std::string& msg);
    void   synchronize();
};

} // namespace quartz

#endif // QUARTZ_PARSER_H
