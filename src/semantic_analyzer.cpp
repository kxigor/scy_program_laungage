#include <include/semantic_analyzer.hpp>
#include <include/config.hpp>
#include <include/type.hpp>
#include <string>
#include <type_traits>
#include <variant>

namespace scy {

SemanticResult SemanticAnalyzer::analyze(const Program& program) {
  global_scope_ = make_unique<Scope>();
  current_scope_ = global_scope_.get();
  errors_.clear();

  visit_program(program);

  return SemanticResult{std::move(global_scope_), std::move(errors_)};
}

void SemanticAnalyzer::visit_program(const Program& program) {
  // First pass: register all function signatures and global variables
  // so forward references work.
  for (const auto& decl : program.declarations) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, FunctionDecl>) {
            VectorT<ParameterSymbol> param_symbols;
            for (PosT i = 0; i < arg.params.size(); ++i) {
              param_symbols.emplace_back(
                  ParameterSymbol{arg.params[i].name, arg.params[i].type, i});
            }

            Symbol sym{
                .kind = SymbolKind::Function,
                .data = FunctionSymbol{arg.name, arg.return_type,
                                       std::move(param_symbols)},
                .location = decl->location};

            define_symbol(arg.name, std::move(sym));

          } else if constexpr (std::is_same_v<T, GlobalVarDecl>) {
            Symbol sym{.kind = SymbolKind::Variable,
                       .data = VariableSymbol{arg.name, arg.type},
                       .location = decl->location};

            define_symbol(arg.name, std::move(sym));
          }
        },
        decl->data);
  }

  // Second pass: visit function bodies
  for (const auto& decl : program.declarations) {
    visit_declaration(*decl);
  }
}

void SemanticAnalyzer::visit_declaration(const Declaration& decl) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, FunctionDecl>) {
          visit_function_decl(arg, decl.location);
        } else if constexpr (std::is_same_v<T, GlobalVarDecl>) {
          visit_global_var_decl(arg, decl.location);
        }
      },
      decl.data);
}

void SemanticAnalyzer::visit_function_decl(const FunctionDecl& func,
                                           SourceLocation /*loc*/) {
  push_scope();

  for (PosT i = 0; i < func.params.size(); ++i) {
    Symbol param_sym{.kind = SymbolKind::Parameter,
                     .data = ParameterSymbol{func.params[i].name,
                                             func.params[i].type, i},
                     .location = {}};

    if (not current_scope_->define(func.params[i].name,
                                   std::move(param_sym))) {
      report_error(SemanticErrorKind::RedeclaredVariable,
                   "Duplicate parameter name '" +
                       StringT(func.params[i].name) + "'",
                   {});
    }
  }

  for (const auto& stmt : func.body) {
    visit_statement(*stmt);
  }

  pop_scope();
}

void SemanticAnalyzer::visit_global_var_decl(const GlobalVarDecl& var,
                                             SourceLocation /*loc*/) {
  if (var.initializer) {
    visit_expression(**var.initializer);
  }
}

void SemanticAnalyzer::visit_statement(const Statement& stmt) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, ExpressionStmt>) {
          visit_expression_stmt(arg);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          visit_return_stmt(arg);
        } else if constexpr (std::is_same_v<T, BlockStmt>) {
          visit_block_stmt(arg);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          visit_if_stmt(arg);
        } else if constexpr (std::is_same_v<T, VarDeclStmt>) {
          visit_var_decl_stmt(arg, stmt.location);
        }
      },
      stmt.data);
}

void SemanticAnalyzer::visit_expression_stmt(const ExpressionStmt& stmt) {
  visit_expression(*stmt.expression);
}

void SemanticAnalyzer::visit_return_stmt(const ReturnStmt& stmt) {
  if (stmt.value) {
    visit_expression(**stmt.value);
  }
}

void SemanticAnalyzer::visit_block_stmt(const BlockStmt& stmt) {
  push_scope();

  for (const auto& s : stmt.statements) {
    visit_statement(*s);
  }

  pop_scope();
}

void SemanticAnalyzer::visit_if_stmt(const IfStmt& stmt) {
  visit_expression(*stmt.condition);

  push_scope();
  visit_statement(*stmt.then_branch);
  pop_scope();

  if (stmt.else_branch) {
    push_scope();
    visit_statement(**stmt.else_branch);
    pop_scope();
  }
}

void SemanticAnalyzer::visit_var_decl_stmt(const VarDeclStmt& stmt,
                                           SourceLocation loc) {
  if (stmt.initializer) {
    visit_expression(**stmt.initializer);
  }

  Symbol sym{.kind = SymbolKind::Variable,
             .data = VariableSymbol{stmt.name, stmt.type},
             .location = loc};

  if (not current_scope_->define(stmt.name, std::move(sym))) {
    report_error(SemanticErrorKind::RedeclaredVariable,
                 "Variable '" + StringT(stmt.name) +
                     "' already declared in this scope",
                 loc);
  }
}

void SemanticAnalyzer::visit_expression(const Expression& expr) {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, NumberExpr>) {
          visit_number_expr(arg);
        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          visit_identifier_expr(arg, expr.location);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          visit_unary_expr(arg);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          visit_binary_expr(arg);
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          visit_assign_expr(arg, expr.location);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          visit_call_expr(arg, expr.location);
        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          visit_grouping_expr(arg);
        }
      },
      expr.data);
}

void SemanticAnalyzer::visit_number_expr(const NumberExpr& /*expr*/) {
  // Nothing to resolve
}

void SemanticAnalyzer::visit_identifier_expr(const IdentifierExpr& expr,
                                             SourceLocation loc) {
  const auto* sym = current_scope_->resolve(expr.name);
  if (sym == nullptr) {
    report_error(SemanticErrorKind::UndeclaredVariable,
                 "Use of undeclared identifier '" + StringT(expr.name) + "'",
                 loc);
  }
}

void SemanticAnalyzer::visit_unary_expr(const UnaryExpr& expr) {
  visit_expression(*expr.operand);
}

void SemanticAnalyzer::visit_binary_expr(const BinaryExpr& expr) {
  visit_expression(*expr.left);
  visit_expression(*expr.right);
}

void SemanticAnalyzer::visit_assign_expr(const AssignExpr& expr,
                                         SourceLocation loc) {
  const auto* sym = current_scope_->resolve(expr.name);
  if (sym == nullptr) {
    report_error(SemanticErrorKind::UndeclaredVariable,
                 "Assignment to undeclared variable '" + StringT(expr.name) +
                     "'",
                 loc);
  }

  visit_expression(*expr.value);
}

void SemanticAnalyzer::visit_call_expr(const CallExpr& expr,
                                       SourceLocation loc) {
  const auto* sym = current_scope_->resolve(expr.callee);
  if (sym == nullptr) {
    report_error(SemanticErrorKind::UndeclaredFunction,
                 "Call to undeclared function '" + StringT(expr.callee) + "'",
                 loc);
  }

  for (const auto& arg : expr.arguments) {
    visit_expression(*arg);
  }
}

void SemanticAnalyzer::visit_grouping_expr(const GroupingExpr& expr) {
  visit_expression(*expr.expression);
}

void SemanticAnalyzer::push_scope() {
  current_scope_ = current_scope_->create_child();
}

void SemanticAnalyzer::pop_scope() { current_scope_ = current_scope_->parent(); }

void SemanticAnalyzer::define_symbol(StringViewT name, Symbol symbol) {
  SourceLocation loc = symbol.location;
  if (not current_scope_->define(name, std::move(symbol))) {
    report_error(SemanticErrorKind::RedeclaredVariable,
                 "'" + StringT(name) + "' already declared in this scope", loc);
  }
}

void SemanticAnalyzer::report_error(SemanticErrorKind kind,
                                    const StringT& message,
                                    SourceLocation loc) {
  errors_.emplace_back(SemanticError{kind, message, loc});
}

}  // namespace scy
