#include <include/ast.hpp>
#include <include/config.hpp>
#include <include/parser.hpp>
#include <include/token.hpp>
#include <include/type.hpp>
#include <string>
#include <utility>
#include <variant>

namespace scy {

namespace {

TypeSpec type_spec_from_token(const LocatedToken& tok) {
  switch (tok.token.type) {
    case TokenType::Int:
      return TypeSpec{TypeKind::Int};
    case TokenType::Void:
      return TypeSpec{TypeKind::Void};
    default:
      return TypeSpec{TypeKind::Int};
  }
}

}  // namespace

Program Parser::parse() {
  Program program;

  while (!is_at_end()) {
    try {
      if (auto decl = declaration()) {
        program.declarations.emplace_back(std::move(decl));
      }
    } catch (const ParseError& e) {
      errors_.push_back(e);
      synchronize();
    }
  }

  return program;
}

DeclPtr Parser::declaration() {
  if (not is_type_token()) {
    throw error(peek(), "Expected type (int/void)");
  }

  const auto kType = advance();
  const auto kName = consume(TokenType::Identifier, "Expected identifier");

  if (match(TokenType::LParen)) {
    return function_declaration(kType, kName);
  }

  return variable_declaration(kType, kName);
}

DeclPtr Parser::function_declaration(LocatedToken type, LocatedToken name) {
  VectorT<Parameter> params;

  if (not check(TokenType::RParen)) {
    do {
      if (not is_type_token()) {
        throw error(peek(), "Expected parameter type");
      }
      const auto kParamType = advance();
      const auto kParamName =
          consume(TokenType::Identifier, "Expected parameter name");
      params.emplace_back(type_spec_from_token(kParamType),
                          kParamName.token.lexem);
    } while (match(TokenType::Comma));
  }

  consume(TokenType::RParen, "Expected ')' after parameters");
  consume(TokenType::LBrace, "Expected '{' before function body");

  VectorT<StmtPtr> body;
  while (!check(TokenType::RBrace) && !is_at_end()) {
    body.emplace_back(statement());
  }

  consume(TokenType::RBrace, "Expected '}' after function body");

  return make_unique<Declaration>(
      FunctionDecl{type_spec_from_token(type), name.token.lexem,
                   std::move(params), std::move(body)},
      type.location);
}

DeclPtr Parser::variable_declaration(LocatedToken type, LocatedToken name) {
  OptionalT<ExprPtr> initializer;

  if (match(TokenType::Assign)) {
    initializer = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after variable declaration");

  return make_unique<Declaration>(
      GlobalVarDecl{type_spec_from_token(type), name.token.lexem,
                    std::move(initializer)},
      type.location);
}

StmtPtr Parser::statement() {
  if (match(TokenType::If)) {
    return if_statement();
  }
  if (match(TokenType::Return)) {
    return return_statement();
  }
  if (match(TokenType::LBrace)) {
    return block_statement();
  }
  if (is_type_token()) {
    return var_decl_statement();
  }
  return expression_statement();
}

StmtPtr Parser::if_statement() {
  const auto kLocation = previous().location;

  consume(TokenType::LParen, "Expected '(' after 'if'");
  auto condition = expression();
  consume(TokenType::RParen, "Expected ')' after condition");

  auto then_branch = statement();
  OptionalT<StmtPtr> else_branch;

  if (match(TokenType::Else)) {
    else_branch = statement();
  }

  return make_unique<Statement>(
      IfStmt{std::move(condition), std::move(then_branch),
             std::move(else_branch)},
      kLocation);
}

StmtPtr Parser::return_statement() {
  const auto kLocation = previous().location;
  OptionalT<ExprPtr> value;

  if (not check(TokenType::Semicolon)) {
    value = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after return");

  return make_unique<Statement>(ReturnStmt{std::move(value)}, kLocation);
}

StmtPtr Parser::block_statement() {
  const auto kLocation = previous().location;
  VectorT<StmtPtr> statements;
  while (!check(TokenType::RBrace) && !is_at_end()) {
    statements.emplace_back(statement());
  }

  consume(TokenType::RBrace, "Expected '}' after block");

  return make_unique<Statement>(BlockStmt{std::move(statements)}, kLocation);
}

StmtPtr Parser::expression_statement() {
  const auto kLocation = peek().location;
  auto expr = expression();
  consume(TokenType::Semicolon, "Expected ';' after expression");

  return make_unique<Statement>(ExpressionStmt{std::move(expr)}, kLocation);
}

StmtPtr Parser::var_decl_statement() {
  const auto kType = advance();
  const auto kName = consume(TokenType::Identifier, "Expected variable name");

  OptionalT<ExprPtr> initializer;
  if (match(TokenType::Assign)) {
    initializer = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after variable declaration");

  return make_unique<Statement>(
      VarDeclStmt{type_spec_from_token(kType), kName.token.lexem,
                  std::move(initializer)},
      kType.location);
}

ExprPtr Parser::expression() { return assignment(); }

ExprPtr Parser::assignment() {
  auto expr = equality();

  if (match(TokenType::Assign)) {
    const auto kEquals = previous();

    if (auto* ident = std::get_if<IdentifierExpr>(&expr->data)) {
      auto value = assignment();
      return make_unique<Expression>(AssignExpr{ident->name, std::move(value)},
                                     expr->location);
    }

    throw error(kEquals, "Invalid assignment target");
  }

  return expr;
}

ExprPtr Parser::equality() {
  auto expr = comparison();

  while (match(TokenType::Equal, TokenType::NotEqual)) {
    const auto kOp = previous();
    auto right = comparison();
    expr = make_unique<Expression>(
        BinaryExpr{std::move(expr), kOp.token, std::move(right)}, kOp.location);
  }

  return expr;
}

ExprPtr Parser::comparison() {
  auto expr = term();

  while (match(TokenType::Less, TokenType::Greater)) {
    const auto kOp = previous();
    auto right = term();
    expr = make_unique<Expression>(
        BinaryExpr{std::move(expr), kOp.token, std::move(right)}, kOp.location);
  }

  return expr;
}

ExprPtr Parser::term() {
  auto expr = unary();

  while (match(TokenType::Plus, TokenType::Minus)) {
    const auto kOp = previous();
    auto right = unary();
    expr = make_unique<Expression>(
        BinaryExpr{std::move(expr), kOp.token, std::move(right)}, kOp.location);
  }

  return expr;
}

ExprPtr Parser::unary() {
  if (match(TokenType::Not, TokenType::Minus)) {
    const auto kOp = previous();
    auto operand = unary();
    return make_unique<Expression>(UnaryExpr{kOp.token, std::move(operand)},
                                   kOp.location);
  }

  return primary();
}

ExprPtr Parser::primary() {
  if (match(TokenType::Number)) {
    const auto kToken = previous();
    return make_unique<Expression>(NumberExpr{kToken.token.lexem},
                                   kToken.location);
  }

  if (match(TokenType::Identifier)) {
    const auto kName = previous();

    if (match(TokenType::LParen)) {
      return call(kName);
    }

    return make_unique<Expression>(IdentifierExpr{kName.token.lexem},
                                   kName.location);
  }

  if (match(TokenType::LParen)) {
    const auto kLocation = previous().location;
    auto expr = expression();
    consume(TokenType::RParen, "Expected ')' after expression");
    return make_unique<Expression>(GroupingExpr{std::move(expr)}, kLocation);
  }

  throw error(peek(), "Expected expression");
}

ExprPtr Parser::call(LocatedToken name) {
  VectorT<ExprPtr> arguments;

  if (not check(TokenType::RParen)) {
    do {
      arguments.emplace_back(expression());
    } while (match(TokenType::Comma));
  }

  consume(TokenType::RParen, "Expected ')' after arguments");

  return make_unique<Expression>(
      CallExpr{name.token.lexem, std::move(arguments)}, name.location);
}

bool Parser::is_at_end() const noexcept {
  return peek().token.type == TokenType::Eof;
}
LocatedToken Parser::peek() const noexcept { return tokens_[current_]; }

LocatedToken Parser::previous() const noexcept { return tokens_[current_ - 1]; }

LocatedToken Parser::advance() {
  if (not is_at_end()) {
    ++current_;
  }
  return previous();
}

bool Parser::check(TokenType type) const noexcept {
  if (is_at_end()) {
    return false;
  }
  return peek().token.type == type;
}

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

LocatedToken Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) {
    return advance();
  }
  throw error(peek(), message);
}

bool Parser::is_type_token() const noexcept {
  return check(TokenType::Int) || check(TokenType::Void);
}

ParseError Parser::error(const LocatedToken& token,
                         const std::string& message) {
  std::string full_message = "Line " + std::to_string(token.location.line) +
                             ", Col " + std::to_string(token.location.column) +
                             ": " + message;
  if (token.token.type != TokenType::Eof) {
    full_message += " (got '" + std::string(token.token.lexem) + "')";
  }
  return {full_message, token.location};
}

void Parser::synchronize() {
  advance();

  while (not is_at_end()) {
    if (previous().token.type == TokenType::Semicolon) {
      return;
    }

    switch (peek().token.type) {
      case TokenType::Int:
      case TokenType::Void:
      case TokenType::If:
      case TokenType::Return:
        return;
      default:
        break;
    }
    advance();
  }
}

}  // namespace scy
