#pragma once
#ifndef QUARTZ_AST_H
#define QUARTZ_AST_H

#include "quartz.h"

namespace quartz {
namespace ast {

// ============================================================
//  AST NODE TYPES
// ============================================================
enum class NodeType : uint32_t {
    // Program root
    PROGRAM,

    // Declarations
    MAIN_DECL,
    FUNC_DECL,
    VAR_DECL,
    IMPORT_DECL,
    USING_DECL,

    // Statements
    PRINT_STMT,
    INPUT_STMT,
    ASSIGN_STMT,
    SAVE_STMT,
    POINT_STMT,
    SPLIT_STMT,
    MOV_STMT,
    ADD_STMT,
    WAIT_STMT,
    RETURN_STMT,
    UTIL_STMT,
    GPU_RENDER_STMT,

    // Control flow
    IF_STMT,
    ELSE_STMT,
    WHEN_STMT,
    FROM_STMT,
    THEN_STMT,
    REVERSE_STMT,

    // Expressions
    BINARY_EXPR,
    UNARY_EXPR,
    CALL_EXPR,
    INDEX_EXPR,
    MEMBER_EXPR,
    LITERAL_EXPR,
    IDENT_EXPR,
    ARRAY_EXPR,
    DICT_EXPR,

    // Math/Logic
    MATH_EXPR,
    LOGIC_EXPR,

    // Memory
    MEM_ALLOC_STMT,
    MEM_FREE_STMT,

    // GPU
    GPU_KERNEL_STMT,
    GPU_LAUNCH_STMT,

    // Hardware
    HW_IO_STMT,
    HW_DMA_STMT,

    // ASM inline
    ASM_STMT,

    // Block
    BLOCK,
    EMPTY
};

// ============================================================
//  BASE AST NODE
// ============================================================
struct Node {
    NodeType type;
    uint32_t line;
    uint32_t col;

    explicit Node(NodeType t, uint32_t l = 0, uint32_t c = 0)
        : type(t), line(l), col(c) {}
    virtual ~Node() = default;

    virtual std::string debug_str() const = 0;
};

using NodePtr = std::unique_ptr<Node>;
using NodeList = std::vector<NodePtr>;

// ============================================================
//  LITERAL NODE
// ============================================================
struct LiteralNode : Node {
    QuartzValue value;

    LiteralNode(QuartzValue v, uint32_t l, uint32_t c)
        : Node(NodeType::LITERAL_EXPR, l, c), value(std::move(v)) {}

    std::string debug_str() const override { return "Literal(" + value.to_string() + ")"; }
};

// ============================================================
//  IDENTIFIER NODE
// ============================================================
struct IdentNode : Node {
    std::string name;

    IdentNode(std::string n, uint32_t l, uint32_t c)
        : Node(NodeType::IDENT_EXPR, l, c), name(std::move(n)) {}

    std::string debug_str() const override { return "Ident(" + name + ")"; }
};

// ============================================================
//  BINARY EXPRESSION
// ============================================================
struct BinaryNode : Node {
    std::string op;
    NodePtr     left;
    NodePtr     right;

    BinaryNode(std::string o, NodePtr l, NodePtr r, uint32_t ln, uint32_t c)
        : Node(NodeType::BINARY_EXPR, ln, c), op(std::move(o)),
          left(std::move(l)), right(std::move(r)) {}

    std::string debug_str() const override {
        return "Binary(" + op + ", " + left->debug_str() + ", " + right->debug_str() + ")";
    }
};

// ============================================================
//  UNARY EXPRESSION
// ============================================================
struct UnaryNode : Node {
    std::string op;
    NodePtr     operand;

    UnaryNode(std::string o, NodePtr operand_, uint32_t l, uint32_t c)
        : Node(NodeType::UNARY_EXPR, l, c), op(std::move(o)), operand(std::move(operand_)) {}

    std::string debug_str() const override {
        return "Unary(" + op + ", " + operand->debug_str() + ")";
    }
};

// ============================================================
//  VARIABLE DECLARATION
// ============================================================
struct VarDeclNode : Node {
    std::string name;
    NodePtr     init;        // optional initializer

    VarDeclNode(std::string n, NodePtr i, uint32_t l, uint32_t c)
        : Node(NodeType::VAR_DECL, l, c), name(std::move(n)), init(std::move(i)) {}

    std::string debug_str() const override {
        return "VarDecl(" + name + (init ? ", " + init->debug_str() : "") + ")";
    }
};

// ============================================================
//  FUNCTION DECLARATION
// ============================================================
struct FuncDeclNode : Node {
    std::string          name;
    std::vector<std::string> params;
    NodeList             body;

    FuncDeclNode(std::string n, std::vector<std::string> p, NodeList b, uint32_t l, uint32_t c)
        : Node(NodeType::FUNC_DECL, l, c), name(std::move(n)),
          params(std::move(p)), body(std::move(b)) {}

    std::string debug_str() const override { return "FuncDecl(" + name + ")"; }
};

// ============================================================
//  CALL EXPRESSION
// ============================================================
struct CallNode : Node {
    std::string  callee;
    NodeList     args;

    CallNode(std::string callee_, NodeList args_, uint32_t l, uint32_t c)
        : Node(NodeType::CALL_EXPR, l, c), callee(std::move(callee_)), args(std::move(args_)) {}

    std::string debug_str() const override { return "Call(" + callee + ")"; }
};

// ============================================================
//  PRINT STATEMENT
// ============================================================
struct PrintNode : Node {
    NodePtr expr;

    PrintNode(NodePtr e, uint32_t l, uint32_t c)
        : Node(NodeType::PRINT_STMT, l, c), expr(std::move(e)) {}

    std::string debug_str() const override {
        return "Print(" + expr->debug_str() + ")";
    }
};

// ============================================================
//  UTIL:: STATEMENT
// ============================================================
struct UtilNode : Node {
    std::string expression_str;   // raw math expression string
    NodePtr     expr;

    UtilNode(std::string e, NodePtr en, uint32_t l, uint32_t c)
        : Node(NodeType::UTIL_STMT, l, c), expression_str(std::move(e)), expr(std::move(en)) {}

    std::string debug_str() const override { return "Util(" + expression_str + ")"; }
};

// ============================================================
//  IF STATEMENT
// ============================================================
struct IfNode : Node {
    NodePtr  condition;
    NodeList then_body;
    NodeList else_body;    // optional

    IfNode(NodePtr cond, NodeList th, NodeList el, uint32_t l, uint32_t c)
        : Node(NodeType::IF_STMT, l, c), condition(std::move(cond)),
          then_body(std::move(th)), else_body(std::move(el)) {}

    std::string debug_str() const override { return "If(" + condition->debug_str() + ")"; }
};

// ============================================================
//  POINT STATEMENT
// ============================================================
struct PointNode : Node {
    std::string target;
    NodePtr     index;

    PointNode(std::string t, NodePtr i, uint32_t l, uint32_t c)
        : Node(NodeType::POINT_STMT, l, c), target(std::move(t)), index(std::move(i)) {}

    std::string debug_str() const override { return "Point(" + target + ")"; }
};

// ============================================================
//  SAVE:: STATEMENT
// ============================================================
struct SaveNode : Node {
    std::string dest;
    std::string key;
    std::string src;

    SaveNode(std::string d, std::string k, std::string s, uint32_t l, uint32_t c)
        : Node(NodeType::SAVE_STMT, l, c), dest(std::move(d)), key(std::move(k)), src(std::move(s)) {}

    std::string debug_str() const override { return "Save(" + dest + " = " + key + " = " + src + ")"; }
};

// ============================================================
//  SPLIT STATEMENT
// ============================================================
struct SplitNode : Node {
    std::string target;

    SplitNode(std::string t, uint32_t l, uint32_t c)
        : Node(NodeType::SPLIT_STMT, l, c), target(std::move(t)) {}

    std::string debug_str() const override { return "Split(" + target + ")"; }
};

// ============================================================
//  PROGRAM ROOT
// ============================================================
struct ProgramNode : Node {
    NodeList body;

    ProgramNode() : Node(NodeType::PROGRAM, 0, 0) {}

    std::string debug_str() const override { return "Program"; }
};

// ============================================================
//  MAIN DECL
// ============================================================
struct MainDeclNode : Node {
    NodeList body;

    MainDeclNode(NodeList b, uint32_t l, uint32_t c)
        : Node(NodeType::MAIN_DECL, l, c), body(std::move(b)) {}

    std::string debug_str() const override { return "MainDecl"; }
};

// ============================================================
//  BLOCK NODE
// ============================================================
struct BlockNode : Node {
    NodeList stmts;

    BlockNode(NodeList s, uint32_t l, uint32_t c)
        : Node(NodeType::BLOCK, l, c), stmts(std::move(s)) {}

    std::string debug_str() const override { return "Block"; }
};

// ============================================================
//  MATH EXPRESSION NODE
// ============================================================
struct MathNode : Node {
    TokenType   operation;
    NodeList    operands;

    MathNode(TokenType op, NodeList ops, uint32_t l, uint32_t c)
        : Node(NodeType::MATH_EXPR, l, c), operation(op), operands(std::move(ops)) {}

    std::string debug_str() const override { return "Math"; }
};

// ============================================================
//  MOV STATEMENT
// ============================================================
struct MovNode : Node {
    std::string dest;
    NodePtr     src;

    MovNode(std::string d, NodePtr s, uint32_t l, uint32_t c)
        : Node(NodeType::MOV_STMT, l, c), dest(std::move(d)), src(std::move(s)) {}

    std::string debug_str() const override { return "MOV(" + dest + ")"; }
};

// ============================================================
//  WAIT STATEMENT
// ============================================================
struct WaitNode : Node {
    NodePtr duration;

    WaitNode(NodePtr d, uint32_t l, uint32_t c)
        : Node(NodeType::WAIT_STMT, l, c), duration(std::move(d)) {}

    std::string debug_str() const override { return "Wait"; }
};

// ============================================================
//  RETURN STATEMENT
// ============================================================
struct ReturnNode : Node {
    NodePtr value;

    ReturnNode(NodePtr v, uint32_t l, uint32_t c)
        : Node(NodeType::RETURN_STMT, l, c), value(std::move(v)) {}

    std::string debug_str() const override { return "Return"; }
};

// ============================================================
//  ASM INLINE STATEMENT
// ============================================================
struct AsmNode : Node {
    std::string asm_code;

    AsmNode(std::string code, uint32_t l, uint32_t c)
        : Node(NodeType::ASM_STMT, l, c), asm_code(std::move(code)) {}

    std::string debug_str() const override { return "ASM(" + asm_code + ")"; }
};

// ============================================================
//  GPU RENDER STATEMENT
// ============================================================
struct GpuRenderNode : Node {
    TokenType   render_op;
    NodeList    args;

    GpuRenderNode(TokenType op, NodeList a, uint32_t l, uint32_t c)
        : Node(NodeType::GPU_RENDER_STMT, l, c), render_op(op), args(std::move(a)) {}

    std::string debug_str() const override { return "GPURender"; }
};

// ============================================================
//  MEMORY ALLOC STATEMENT
// ============================================================
struct MemAllocNode : Node {
    std::string target;
    NodePtr     size_expr;
    size_t      alignment = 0;

    MemAllocNode(std::string t, NodePtr s, size_t align, uint32_t l, uint32_t c)
        : Node(NodeType::MEM_ALLOC_STMT, l, c), target(std::move(t)),
          size_expr(std::move(s)), alignment(align) {}

    std::string debug_str() const override { return "MemAlloc(" + target + ")"; }
};

// ============================================================
//  HARDWARE IO STATEMENT
// ============================================================
struct HwIoNode : Node {
    TokenType   hw_op;
    NodeList    args;

    HwIoNode(TokenType op, NodeList a, uint32_t l, uint32_t c)
        : Node(NodeType::HW_IO_STMT, l, c), hw_op(op), args(std::move(a)) {}

    std::string debug_str() const override { return "HwIO"; }
};

// ============================================================
//  IMPORT/USING NODES
// ============================================================
struct ImportNode : Node {
    std::string path;
    std::string alias;

    ImportNode(std::string p, std::string a, uint32_t l, uint32_t c)
        : Node(NodeType::IMPORT_DECL, l, c), path(std::move(p)), alias(std::move(a)) {}

    std::string debug_str() const override { return "Import(" + path + ")"; }
};

struct UsingNode : Node {
    std::string path;

    UsingNode(std::string p, uint32_t l, uint32_t c)
        : Node(NodeType::USING_DECL, l, c), path(std::move(p)) {}

    std::string debug_str() const override { return "Using(" + path + ")"; }
};

// ============================================================
//  ARRAY EXPRESSION
// ============================================================
struct ArrayNode : Node {
    NodeList elements;

    ArrayNode(NodeList e, uint32_t l, uint32_t c)
        : Node(NodeType::ARRAY_EXPR, l, c), elements(std::move(e)) {}

    std::string debug_str() const override { return "Array[" + std::to_string(elements.size()) + "]"; }
};

// ============================================================
//  ADD STATEMENT
// ============================================================
struct AddNode : Node {
    NodePtr left;
    NodePtr right;

    AddNode(NodePtr l, NodePtr r, uint32_t ln, uint32_t c)
        : Node(NodeType::ADD_STMT, ln, c), left(std::move(l)), right(std::move(r)) {}

    std::string debug_str() const override { return "Add"; }
};

} // namespace ast
} // namespace quartz

#endif // QUARTZ_AST_H
