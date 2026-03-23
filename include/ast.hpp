#pragma once

#include <ostream>
#include <utility>

#include "config.hpp"
#include "token.hpp"
#include "type.hpp"

namespace scy {

/*=================== Forward declarations ===================*/
struct Expression;
struct Statement;
struct Declaration;

using ExprPtr = UniquePtrT<Expression>;
using StmtPtr = UniquePtrT<Statement>;
using DeclPtr = UniquePtrT<Declaration>;

/*======================= Expressions ========================*/

struct NumberExpr {
  explicit NumberExpr(StringViewT literal);

  StringViewT literal;
};

struct IdentifierExpr {
  explicit IdentifierExpr(StringViewT name);

  StringViewT name;
};

struct UnaryExpr {
  UnaryExpr(Token op, ExprPtr operand);

  Token op;
  ExprPtr operand;
};

struct BinaryExpr {
  BinaryExpr(ExprPtr left, Token op, ExprPtr right);

  ExprPtr left;
  Token op;
  ExprPtr right;
};

struct AssignExpr {
  AssignExpr(StringViewT name, ExprPtr value);

  StringViewT name;
  ExprPtr value;
};

struct CallExpr {
  CallExpr(StringViewT callee, VectorT<ExprPtr> args);

  StringViewT callee;
  VectorT<ExprPtr> arguments;
};

struct GroupingExpr {
  explicit GroupingExpr(ExprPtr expr);

  ExprPtr expression;
};

using ExprVariant = VariantT<NumberExpr, IdentifierExpr, UnaryExpr, BinaryExpr,
                             AssignExpr, CallExpr, GroupingExpr>;

struct Expression {
  template <typename T>
  Expression(T&& expr, SourceLocation loc);

  ExprVariant data;
  SourceLocation location;
};

/*======================== Statements ========================*/

struct ExpressionStmt {
  explicit ExpressionStmt(ExprPtr expr);

  ExprPtr expression;
};

struct ReturnStmt {
  explicit ReturnStmt(OptionalT<ExprPtr> val);

  OptionalT<ExprPtr> value;
};

struct BlockStmt {
  explicit BlockStmt(VectorT<StmtPtr> stmts);

  VectorT<StmtPtr> statements;
};

struct IfStmt {
  IfStmt(ExprPtr cond, StmtPtr then_b, OptionalT<StmtPtr> else_b);

  ExprPtr condition;
  StmtPtr then_branch;
  OptionalT<StmtPtr> else_branch;
};

struct VarDeclStmt {
  VarDeclStmt(TypeSpec type, StringViewT name, OptionalT<ExprPtr> init);

  TypeSpec type;
  StringViewT name;
  OptionalT<ExprPtr> initializer;
};

using StmtVariant =
    VariantT<ExpressionStmt, ReturnStmt, BlockStmt, IfStmt, VarDeclStmt>;

struct Statement {
  template <typename T>
  Statement(T&& stmt, SourceLocation loc);

  StmtVariant data;
  SourceLocation location;
};

/*======================= Declarations =======================*/

struct Parameter {
  Parameter(TypeSpec type, StringViewT name);

  TypeSpec type;
  StringViewT name;
};

struct FunctionDecl {
  FunctionDecl(TypeSpec return_type, StringViewT name,
               VectorT<Parameter> params, VectorT<StmtPtr> body);

  TypeSpec return_type;
  StringViewT name;
  VectorT<Parameter> params;
  VectorT<StmtPtr> body;
};

struct GlobalVarDecl {
  GlobalVarDecl(TypeSpec type, StringViewT name, OptionalT<ExprPtr> init);

  TypeSpec type;
  StringViewT name;
  OptionalT<ExprPtr> initializer;
};

using DeclVariant = VariantT<FunctionDecl, GlobalVarDecl>;

struct Declaration {
  template <typename T>
  Declaration(T&& decl, SourceLocation loc);

  DeclVariant data;
  SourceLocation location;
};

/*========================= AST Root =========================*/

struct Program {
  Program();
  explicit Program(VectorT<DeclPtr> decls);

  VectorT<DeclPtr> declarations;
};

/*======================= AST Printing =======================*/
void print_ast(std::ostream& os, const Program& program, int indent = 0);
void print_ast(std::ostream& os, const Declaration& decl, int indent = 0);
void print_ast(std::ostream& os, const Statement& stmt, int indent = 0);
void print_ast(std::ostream& os, const Expression& expr, int indent = 0);

template <typename T>
Expression::Expression(T&& expr, SourceLocation loc)
    : data(std::forward<T>(expr)), location(loc) {}

template <typename T>
Statement::Statement(T&& stmt, SourceLocation loc)
    : data(std::forward<T>(stmt)), location(loc) {}

template <typename T>
Declaration::Declaration(T&& decl, SourceLocation loc)
    : data(std::forward<T>(decl)), location(loc) {}

}  // namespace scy