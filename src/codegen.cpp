#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <include/ast.hpp>
#include <include/codegen.hpp>
#include <include/config.hpp>
#include <include/semantic_analyzer.hpp>
#include <include/token.hpp>
#include <include/type.hpp>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace scy {

CodeGen::CodeGen(const StringT& module_name)
    : module_(std::make_unique<llvm::Module>(module_name, context_)),
      builder_(context_) {}

bool CodeGen::generate(const Program& program,
                       const SemanticResult& /*unused*/) {
  errors_.clear();

  declare_functions(program);

  for (const auto& decl : program.declarations) {
    visit_declaration(*decl);
    if (has_errors()) {
      return false;
    }
  }

  std::string verify_err;
  llvm::raw_string_ostream verify_os(verify_err);
  if (llvm::verifyModule(*module_, &verify_os)) {
    report_error(std::format("Module verification failed: {}", verify_err));
    return false;
  }

  return true;
}

bool CodeGen::dump_to_file(const StringT& filename) const {
  std::error_code ec;
  llvm::raw_fd_ostream file(filename, ec);
  if (ec) {
    return false;
  }
  module_->print(file, nullptr);
  return true;
}

void CodeGen::print_ir(llvm::raw_ostream& os) const {
  module_->print(os, nullptr);
}

void CodeGen::visit_declaration(const Declaration& decl) {
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

void CodeGen::visit_function_decl(const FunctionDecl& func,
                                  SourceLocation /*loc*/) {
  llvm::Function* llvm_func = module_->getFunction(StringT(func.name));
  if (llvm_func == nullptr) {
    report_error(std::format("Function '{}' not found in module", func.name));
    return;
  }

  auto* entry_bb = llvm::BasicBlock::Create(context_, "entry", llvm_func);
  builder_.SetInsertPoint(entry_bb);

  push_named_values();

  PosT idx = 0;
  for (auto& arg : llvm_func->args()) {
    const auto& param = func.params[idx];
    arg.setName(StringT(param.name));

    auto* alloca = create_entry_block_alloca(llvm_func, arg.getType(),
                                             StringT(param.name));
    builder_.CreateStore(&arg, alloca);
    set_named_value(param.name, alloca);
    ++idx;
  }

  for (const auto& stmt : func.body) {
    visit_statement(*stmt);
  }

  llvm::BasicBlock* current_bb = builder_.GetInsertBlock();
  if (current_bb != nullptr && current_bb->getTerminator() == nullptr) {
    if (func.return_type.kind == TypeKind::Void) {
      builder_.CreateRetVoid();
    } else {
      builder_.CreateRet(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0));
    }
  }

  pop_named_values();
}

void CodeGen::visit_global_var_decl(const GlobalVarDecl& var,
                                    SourceLocation /*loc*/) {
  llvm::Type* var_type = to_llvm_type(var.type);

  llvm::Constant* init_val = nullptr;
  if (var.initializer) {
    if (const auto* num = std::get_if<NumberExpr>(&(*var.initializer)->data)) {
      const int kValue = std::stoi(StringT(num->literal));
      init_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                        static_cast<uint64_t>(kValue), true);
    }
  }

  if (init_val == nullptr) {
    init_val = llvm::Constant::getNullValue(var_type);
  }

  auto* global_var = new llvm::GlobalVariable(
      *module_, var_type, false, llvm::GlobalValue::ExternalLinkage, init_val,
      StringT(var.name));

  globals_[var.name] = global_var;
}

void CodeGen::visit_statement(const Statement& stmt) {
  if (builder_.GetInsertBlock() != nullptr &&
      builder_.GetInsertBlock()->getTerminator() != nullptr) {
    return;
  }

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
          visit_var_decl_stmt(arg);
        }
      },
      stmt.data);
}

void CodeGen::visit_expression_stmt(const ExpressionStmt& stmt) {
  visit_expression(*stmt.expression);
}

void CodeGen::visit_return_stmt(const ReturnStmt& stmt) {
  if (stmt.value) {
    llvm::Value* ret_val = visit_expression(**stmt.value);
    if (ret_val != nullptr) {
      builder_.CreateRet(ret_val);
    }
  } else {
    builder_.CreateRetVoid();
  }
}

void CodeGen::visit_block_stmt(const BlockStmt& stmt) {
  push_named_values();

  for (const auto& s : stmt.statements) {
    visit_statement(*s);
  }

  pop_named_values();
}

void CodeGen::visit_if_stmt(const IfStmt& stmt) {
  llvm::Value* cond_val = visit_expression(*stmt.condition);
  if (cond_val == nullptr) {
    return;
  }

  cond_val = builder_.CreateICmpNE(
      cond_val, llvm::ConstantInt::get(cond_val->getType(), 0), "ifcond");

  llvm::Function* func = builder_.GetInsertBlock()->getParent();

  auto* then_bb = llvm::BasicBlock::Create(context_, "then", func);
  auto* else_bb = llvm::BasicBlock::Create(context_, "else", func);
  auto* merge_bb = llvm::BasicBlock::Create(context_, "ifcont", func);

  builder_.CreateCondBr(cond_val, then_bb, else_bb);

  builder_.SetInsertPoint(then_bb);
  visit_statement(*stmt.then_branch);
  if (builder_.GetInsertBlock()->getTerminator() == nullptr) {
    builder_.CreateBr(merge_bb);
  }

  builder_.SetInsertPoint(else_bb);
  if (stmt.else_branch) {
    visit_statement(**stmt.else_branch);
  }
  if (builder_.GetInsertBlock()->getTerminator() == nullptr) {
    builder_.CreateBr(merge_bb);
  }

  builder_.SetInsertPoint(merge_bb);
}

void CodeGen::visit_var_decl_stmt(const VarDeclStmt& stmt) {
  llvm::Function* func = builder_.GetInsertBlock()->getParent();
  llvm::Type* var_type = to_llvm_type(stmt.type);

  auto* alloca = create_entry_block_alloca(func, var_type, StringT(stmt.name));

  if (stmt.initializer) {
    llvm::Value* init_val = visit_expression(**stmt.initializer);
    if (init_val != nullptr) {
      builder_.CreateStore(init_val, alloca);
    }
  } else {
    builder_.CreateStore(llvm::Constant::getNullValue(var_type), alloca);
  }

  set_named_value(stmt.name, alloca);
}

llvm::Value* CodeGen::visit_expression(const Expression& expr) {
  return std::visit(
      [&](auto&& arg) -> llvm::Value* {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, NumberExpr>) {
          return visit_number_expr(arg);
        } else if constexpr (std::is_same_v<T, IdentifierExpr>) {
          return visit_identifier_expr(arg);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
          return visit_unary_expr(arg);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
          return visit_binary_expr(arg);
        } else if constexpr (std::is_same_v<T, AssignExpr>) {
          return visit_assign_expr(arg);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
          return visit_call_expr(arg);
        } else if constexpr (std::is_same_v<T, GroupingExpr>) {
          return visit_grouping_expr(arg);
        }

        return nullptr;
      },
      expr.data);
}

llvm::Value* CodeGen::visit_number_expr(const NumberExpr& expr) {
  const int kValue = std::stoi(StringT(expr.literal));
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                static_cast<uint64_t>(kValue), true);
}

llvm::Value* CodeGen::visit_identifier_expr(const IdentifierExpr& expr) {
  if (auto* alloca = get_named_value(expr.name)) {
    return builder_.CreateLoad(alloca->getAllocatedType(), alloca,
                               StringT(expr.name));
  }

  auto it = globals_.find(expr.name);
  if (it != globals_.end()) {
    return builder_.CreateLoad(it->second->getValueType(), it->second,
                               StringT(expr.name));
  }

  report_error(std::format("Unknown variable: {}", expr.name));
  return nullptr;
}

llvm::Value* CodeGen::visit_unary_expr(const UnaryExpr& expr) {
  llvm::Value* operand = visit_expression(*expr.operand);
  if (operand == nullptr) {
    return nullptr;
  }

  switch (expr.op.type) {
    case TokenType::Minus:
      return builder_.CreateNeg(operand, "neg");
    case TokenType::Not: {
      auto* zero = llvm::ConstantInt::get(operand->getType(), 0);
      auto* cmp = builder_.CreateICmpEQ(operand, zero, "nottmp");
      return builder_.CreateZExt(cmp, llvm::Type::getInt32Ty(context_),
                                 "notres");
    }
    default:
      report_error(std::format("Unknown unary operator: {}", expr.op.lexem));
      return nullptr;
  }
}

llvm::Value* CodeGen::visit_binary_expr(const BinaryExpr& expr) {
  llvm::Value* lhs = visit_expression(*expr.left);
  llvm::Value* rhs = visit_expression(*expr.right);
  if (lhs == nullptr || rhs == nullptr) {
    return nullptr;
  }

  switch (expr.op.type) {
    case TokenType::Plus:
      return builder_.CreateAdd(lhs, rhs, "addtmp");
    case TokenType::Minus:
      return builder_.CreateSub(lhs, rhs, "subtmp");
    case TokenType::Equal: {
      auto* cmp = builder_.CreateICmpEQ(lhs, rhs, "eqtmp");
      return builder_.CreateZExt(cmp, llvm::Type::getInt32Ty(context_),
                                 "eqres");
    }
    case TokenType::NotEqual: {
      auto* cmp = builder_.CreateICmpNE(lhs, rhs, "netmp");
      return builder_.CreateZExt(cmp, llvm::Type::getInt32Ty(context_),
                                 "neres");
    }
    case TokenType::Less: {
      auto* cmp = builder_.CreateICmpSLT(lhs, rhs, "lttmp");
      return builder_.CreateZExt(cmp, llvm::Type::getInt32Ty(context_),
                                 "ltres");
    }
    case TokenType::Greater: {
      auto* cmp = builder_.CreateICmpSGT(lhs, rhs, "gttmp");
      return builder_.CreateZExt(cmp, llvm::Type::getInt32Ty(context_),
                                 "gtres");
    }
    default:
      report_error(std::format("Unknown binary operator: {}", expr.op.lexem));
      return nullptr;
  }
}

llvm::Value* CodeGen::visit_assign_expr(const AssignExpr& expr) {
  llvm::Value* val = visit_expression(*expr.value);
  if (val == nullptr) {
    return nullptr;
  }

  if (auto* alloca = get_named_value(expr.name)) {
    builder_.CreateStore(val, alloca);
    return val;
  }

  auto it = globals_.find(expr.name);
  if (it != globals_.end()) {
    builder_.CreateStore(val, it->second);
    return val;
  }

  report_error(std::format("Unknown variable for assignment: {}", expr.name));
  return nullptr;
}

llvm::Value* CodeGen::visit_call_expr(const CallExpr& expr) {
  llvm::Function* callee = module_->getFunction(StringT(expr.callee));
  if (callee == nullptr) {
    report_error(std::format("Unknown function: {}", expr.callee));
    return nullptr;
  }

  VectorT<llvm::Value*> args;
  args.reserve(expr.arguments.size());
  for (const auto& arg : expr.arguments) {
    llvm::Value* const kArgVal = visit_expression(*arg);  // NOLINT
    if (kArgVal == nullptr) {
      return nullptr;
    }
    args.push_back(kArgVal);
  }

  if (callee->getReturnType()->isVoidTy()) {
    builder_.CreateCall(callee, args);
    return nullptr;
  }

  return builder_.CreateCall(callee, args, "calltmp");
}

llvm::Value* CodeGen::visit_grouping_expr(const GroupingExpr& expr) {
  return visit_expression(*expr.expression);
}

llvm::Type* CodeGen::to_llvm_type(const TypeSpec& type) {
  switch (type.kind) {
    case TypeKind::Int:
      return llvm::Type::getInt32Ty(context_);
    case TypeKind::Void:
      return llvm::Type::getVoidTy(context_);
    default:
      std::unreachable();
  }
  return llvm::Type::getVoidTy(context_);
}

llvm::FunctionType* CodeGen::to_llvm_function_type(const FunctionDecl& func) {
  llvm::Type* ret_type = to_llvm_type(func.return_type);

  VectorT<llvm::Type*> param_types;
  param_types.reserve(func.params.size());
  for (const auto& param : func.params) {
    param_types.push_back(to_llvm_type(param.type));
  }

  return llvm::FunctionType::get(ret_type, param_types, false);
}

void CodeGen::declare_functions(const Program& program) {
  for (const auto& decl : program.declarations) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, FunctionDecl>) {
            auto* func_type = to_llvm_function_type(arg);
            llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                                   StringT(arg.name), module_.get());
          }
        },
        decl->data);
  }
}

void CodeGen::push_named_values() { named_values_stack_.emplace_back(); }

void CodeGen::pop_named_values() { named_values_stack_.pop_back(); }

void CodeGen::set_named_value(StringViewT name, llvm::AllocaInst* alloca) {
  if (not named_values_stack_.empty()) {
    named_values_stack_.back()[name] = alloca;
  }
}

llvm::AllocaInst* CodeGen::get_named_value(StringViewT name) {
  for (auto it = named_values_stack_.rbegin(); it != named_values_stack_.rend();
       ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}

void CodeGen::report_error(const StringT& message) {
  errors_.emplace_back(message);
}

llvm::AllocaInst* CodeGen::create_entry_block_alloca(llvm::Function* func,
                                                     llvm::Type* type,
                                                     const StringT& name) {
  llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(),
                                func->getEntryBlock().begin());
  return tmp_builder.CreateAlloca(type, nullptr, name);
}

}  // namespace scy
