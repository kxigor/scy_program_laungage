#pragma once

#include "ast.hpp"
#include "config.hpp"
#include "scope.hpp"
#include "semantic_error.hpp"
#include "symbol.hpp"

namespace scy {

struct SemanticResult {
  UniquePtrT<Scope> global_scope;
  VectorT<SemanticError> errors;
};

class SemanticAnalyzer {
 public:
  SemanticResult analyze(const Program& program);

 private:
  void visit_program(const Program& program);
  void visit_declaration(const Declaration& decl);
  void visit_function_decl(const FunctionDecl& func, SourceLocation loc);
  void visit_global_var_decl(const GlobalVarDecl& var, SourceLocation loc);

  void visit_statement(const Statement& stmt);
  void visit_expression_stmt(const ExpressionStmt& stmt);
  void visit_return_stmt(const ReturnStmt& stmt);
  void visit_block_stmt(const BlockStmt& stmt);
  void visit_if_stmt(const IfStmt& stmt);
  void visit_var_decl_stmt(const VarDeclStmt& stmt, SourceLocation loc);

  void visit_expression(const Expression& expr);
  void visit_number_expr(const NumberExpr& expr);
  void visit_identifier_expr(const IdentifierExpr& expr, SourceLocation loc);
  void visit_unary_expr(const UnaryExpr& expr);
  void visit_binary_expr(const BinaryExpr& expr);
  void visit_assign_expr(const AssignExpr& expr, SourceLocation loc);
  void visit_call_expr(const CallExpr& expr, SourceLocation loc);
  void visit_grouping_expr(const GroupingExpr& expr);

  void push_scope();
  void pop_scope();

  void define_symbol(StringViewT name, Symbol symbol);
  void report_error(SemanticErrorKind kind, const StringT& message,
                    SourceLocation loc);

  Scope* current_scope_{nullptr};
  UniquePtrT<Scope> global_scope_;
  VectorT<SemanticError> errors_;
};

}  // namespace scy
