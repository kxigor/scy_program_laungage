#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "ast.hpp"
#include "config.hpp"
#include "semantic_analyzer.hpp"
#include "type.hpp"

namespace scy {

class CodeGen {
 public:
  /*================= Constructors/Destructors =================*/
  explicit CodeGen(const StringT& module_name = "scy_module");

  CodeGen(const CodeGen& /*unused*/) = default;

  CodeGen(CodeGen&& /*unused*/) = default;

  ~CodeGen() = default;

  /*======================== Assignment ========================*/
  CodeGen& operator=(const CodeGen& /*unused*/) = default;

  CodeGen& operator=(CodeGen&& /*unused*/) = default;

  /*========================= Core API =========================*/
  [[nodiscard]] bool generate(const Program& program,
                              const SemanticResult& sema_result);

  [[nodiscard]] bool dump_to_file(const StringT& filename) const;

  /*========================= Getters ==========================*/
  [[nodiscard]] llvm::Module& module() noexcept { return *module_; }

  [[nodiscard]] const llvm::Module& module() const noexcept { return *module_; }

  [[nodiscard]] const VectorT<StringT>& errors() const noexcept {
    return errors_;
  }

  /*========================== Status ==========================*/
  [[nodiscard]] bool has_errors() const noexcept { return not errors_.empty(); }

  /*========================= Printer ==========================*/
  void print_ir(llvm::raw_ostream& os) const;

 private:
  /*========================== Impls ===========================*/
  void visit_declaration(const Declaration& decl);
  void visit_function_decl(const FunctionDecl& func, SourceLocation loc);
  void visit_global_var_decl(const GlobalVarDecl& var, SourceLocation loc);

  void visit_statement(const Statement& stmt);
  void visit_expression_stmt(const ExpressionStmt& stmt);
  void visit_return_stmt(const ReturnStmt& stmt);
  void visit_block_stmt(const BlockStmt& stmt);
  void visit_if_stmt(const IfStmt& stmt);
  void visit_var_decl_stmt(const VarDeclStmt& stmt);

  llvm::Value* visit_expression(const Expression& expr);
  llvm::Value* visit_number_expr(const NumberExpr& expr);
  llvm::Value* visit_identifier_expr(const IdentifierExpr& expr);
  llvm::Value* visit_unary_expr(const UnaryExpr& expr);
  llvm::Value* visit_binary_expr(const BinaryExpr& expr);
  llvm::Value* visit_assign_expr(const AssignExpr& expr);
  llvm::Value* visit_call_expr(const CallExpr& expr);
  llvm::Value* visit_grouping_expr(const GroupingExpr& expr);

  llvm::Type* to_llvm_type(const TypeSpec& type);
  llvm::FunctionType* to_llvm_function_type(const FunctionDecl& func);

  void declare_functions(const Program& program);

  void push_named_values();
  void pop_named_values();
  void set_named_value(StringViewT name, llvm::AllocaInst* alloca);
  llvm::AllocaInst* get_named_value(StringViewT name);

  void report_error(const StringT& message);

  static llvm::AllocaInst* create_entry_block_alloca(llvm::Function* func,
                                                     llvm::Type* type,
                                                     const StringT& name);

  /*======================= Data fileds ========================*/
  llvm::LLVMContext context_;
  UniquePtrT<llvm::Module> module_;
  llvm::IRBuilder<> builder_;

  VectorT<UmapT<StringViewT, llvm::AllocaInst*>> named_values_stack_;

  UmapT<StringViewT, llvm::GlobalVariable*> globals_;

  VectorT<StringT> errors_;
};

}  // namespace scy
