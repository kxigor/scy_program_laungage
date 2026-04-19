#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "ast.hpp"
#include "config.hpp"
#include "scope.hpp"
#include "semantic_analyzer.hpp"

namespace scy {

class CodeGen {
 public:
  CodeGen(const StringT& module_name = "scy_module");

  /// Generates LLVM IR from the program AST. Returns true on success.
  bool generate(const Program& program, const SemanticResult& sema_result);

  /// Prints the generated IR to the given stream.
  void print_ir(llvm::raw_ostream& os) const;

  /// Dumps IR to a .ll file. Returns true on success.
  bool dump_to_file(const StringT& filename) const;

  [[nodiscard]] llvm::Module& module() noexcept { return *module_; }
  [[nodiscard]] const llvm::Module& module() const noexcept { return *module_; }

  [[nodiscard]] const VectorT<StringT>& errors() const noexcept {
    return errors_;
  }

  [[nodiscard]] bool has_errors() const noexcept { return not errors_.empty(); }

 private:
  // Declaration visitors
  void visit_declaration(const Declaration& decl);
  void visit_function_decl(const FunctionDecl& func, SourceLocation loc);
  void visit_global_var_decl(const GlobalVarDecl& var, SourceLocation loc);

  // Statement visitors
  void visit_statement(const Statement& stmt);
  void visit_expression_stmt(const ExpressionStmt& stmt);
  void visit_return_stmt(const ReturnStmt& stmt);
  void visit_block_stmt(const BlockStmt& stmt);
  void visit_if_stmt(const IfStmt& stmt);
  void visit_var_decl_stmt(const VarDeclStmt& stmt);

  // Expression visitors — return the llvm::Value* produced
  llvm::Value* visit_expression(const Expression& expr);
  llvm::Value* visit_number_expr(const NumberExpr& expr);
  llvm::Value* visit_identifier_expr(const IdentifierExpr& expr);
  llvm::Value* visit_unary_expr(const UnaryExpr& expr);
  llvm::Value* visit_binary_expr(const BinaryExpr& expr);
  llvm::Value* visit_assign_expr(const AssignExpr& expr);
  llvm::Value* visit_call_expr(const CallExpr& expr);
  llvm::Value* visit_grouping_expr(const GroupingExpr& expr);

  // Type mapping
  llvm::Type* to_llvm_type(const TypeSpec& type);
  llvm::FunctionType* to_llvm_function_type(const FunctionDecl& func);

  // Forward-declare all functions (first pass)
  void declare_functions(const Program& program);

  // Named values (local variable alloca map per scope)
  void push_named_values();
  void pop_named_values();
  void set_named_value(StringViewT name, llvm::AllocaInst* alloca);
  llvm::AllocaInst* get_named_value(StringViewT name);

  // Helper: create alloca at function entry
  llvm::AllocaInst* create_entry_block_alloca(llvm::Function* func,
                                              llvm::Type* type,
                                              const StringT& name);

  void report_error(const StringT& message);

  llvm::LLVMContext context_;
  UniquePtrT<llvm::Module> module_;
  llvm::IRBuilder<> builder_;

  // Stack of named-value scopes (StringViewT -> AllocaInst*)
  VectorT<UmapT<StringViewT, llvm::AllocaInst*>> named_values_stack_;

  // Global variable map
  UmapT<StringViewT, llvm::GlobalVariable*> globals_;

  VectorT<StringT> errors_;
};

}  // namespace scy
