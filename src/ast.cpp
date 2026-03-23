#include <include/ast.hpp>
#include <include/config.hpp>
#include <include/token.hpp>
#include <include/type.hpp>
#include <utility>

namespace scy {

NumberExpr::NumberExpr(StringViewT literal) : literal(std::move(literal)) {}

IdentifierExpr::IdentifierExpr(StringViewT name) : name(std::move(name)) {}

UnaryExpr::UnaryExpr(Token op, ExprPtr operand)
    : op(std::move(op)), operand(std::move(operand)) {}

BinaryExpr::BinaryExpr(ExprPtr left, Token op, ExprPtr right)
    : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}

AssignExpr::AssignExpr(StringViewT name, ExprPtr value)
    : name(std::move(name)), value(std::move(value)) {}

CallExpr::CallExpr(StringViewT callee, VectorT<ExprPtr> args)
    : callee(std::move(callee)), arguments(std::move(args)) {}

GroupingExpr::GroupingExpr(ExprPtr expr) : expression(std::move(expr)) {}

ExpressionStmt::ExpressionStmt(ExprPtr expr) : expression(std::move(expr)) {}

ReturnStmt::ReturnStmt(OptionalT<ExprPtr> val) : value(std::move(val)) {}

BlockStmt::BlockStmt(VectorT<StmtPtr> stmts) : statements(std::move(stmts)) {}

IfStmt::IfStmt(ExprPtr cond, StmtPtr then_b, OptionalT<StmtPtr> else_b)
    : condition(std::move(cond)),
      then_branch(std::move(then_b)),
      else_branch(std::move(else_b)) {}

VarDeclStmt::VarDeclStmt(TypeSpec type, StringViewT name,
                         OptionalT<ExprPtr> init)
    : type(std::move(type)),
      name(std::move(name)),
      initializer(std::move(init)) {}

Parameter::Parameter(TypeSpec type, StringViewT name)
    : type(std::move(type)), name(std::move(name)) {}

FunctionDecl::FunctionDecl(TypeSpec return_type, StringViewT name,
                           VectorT<Parameter> params, VectorT<StmtPtr> body)
    : return_type(std::move(return_type)),
      name(std::move(name)),
      params(std::move(params)),
      body(std::move(body)) {}

GlobalVarDecl::GlobalVarDecl(TypeSpec type, StringViewT name,
                             OptionalT<ExprPtr> init)
    : type(std::move(type)),
      name(std::move(name)),
      initializer(std::move(init)) {}

Program::Program() = default;

Program::Program(VectorT<DeclPtr> decls) : declarations(std::move(decls)) {}

}  // namespace scy