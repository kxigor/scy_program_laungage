#include <include/ast.hpp>
#include <include/config.hpp>
#include <include/parser.hpp>
#include <include/token.hpp>
#include <string>
#include <utility>
#include <variant>

namespace scy {

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

  const Token kType = advance();
  const Token kName = consume(TokenType::Identifier, "Expected identifier");

  if (match(TokenType::LParen)) {
    return function_declaration(kType, kName);
  }

  return variable_declaration(kType, kName);
}

DeclPtr Parser::function_declaration(Token type, Token name) {
  VectorT<Parameter> params;

  if (not check(TokenType::RParen)) {
    do {
      if (not is_type_token()) {
        throw error(peek(), "Expected parameter type");
      }
      const Token kParamType = advance();
      const Token kParamName =
          consume(TokenType::Identifier, "Expected parameter name");
      params.emplace_back(kParamType, kParamName);
    } while (match(TokenType::Comma));
  }

  consume(TokenType::RParen, "Expected ')' after parameters");
  consume(TokenType::LBrace, "Expected '{' before function body");

  VectorT<StmtPtr> body;
  while (!check(TokenType::RBrace) && !is_at_end()) {
    body.emplace_back(statement());
  }

  consume(TokenType::RBrace, "Expected '}' after function body");

  // clang-format off
  return make_unique<Declaration>(
  /*.data = */
    FunctionDecl{
      .return_type  = type,
      .name         = name, 
      .params       = std::move(params), 
      .body         = std::move(body)
    },
  /*.location = */
    type.location
  );
  // clang-format on
}

DeclPtr Parser::variable_declaration(Token type, Token name) {
  OptionalT<ExprPtr> initializer;

  if (match(TokenType::Assign)) {
    initializer = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after variable declaration");

  // clang-format off
  return make_unique<Declaration>(
  /*.data = */
    GlobalVarDecl{
      .type         = type, 
      .name         = name, 
      .initializer  = std::move(initializer)
    }, 
  /*.location = */
    type.location
  );
  // clang-format on
}

StmtPtr Parser::statement() {
  if (match(TokenType::If)) {
    return if_statement();
  }
  if (match(TokenType::Return)) {
    return return_statement();
  }
  if (match(TokenType::Print)) {
    return print_statement();
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
  const SourceLocation kLocation = previous().location;

  consume(TokenType::LParen, "Expected '(' after 'if'");
  ExprPtr condition = expression();
  consume(TokenType::RParen, "Expected ')' after condition");

  StmtPtr then_branch = statement();
  OptionalT<StmtPtr> else_branch;

  if (match(TokenType::Else)) {
    else_branch = statement();
  }

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    IfStmt{
      .condition    = std::move(condition), 
      .then_branch  = std::move(then_branch),
      .else_branch  = std::move(else_branch)
    },
  /*.location = */
    kLocation
  );
  // clang-format on
}

StmtPtr Parser::return_statement() {
  const Token kKeyword = previous();
  OptionalT<ExprPtr> value;

  if (not check(TokenType::Semicolon)) {
    value = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after return");

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    ReturnStmt{
      .keyword  = kKeyword, 
      .value    = std::move(value)
    },
  /*.location = */
    kKeyword.location
  );
  // clang-format on
}

StmtPtr Parser::print_statement() {
  const SourceLocation kLocation = previous().location;
  ExprPtr value = expression();
  consume(TokenType::Semicolon, "Expected ';' after print");

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    PrintStmt{
      .expression = std::move(value)
    }, 
  /*.location = */
    kLocation
  );
  // clang-format on
}

StmtPtr Parser::block_statement() {
  const SourceLocation kLocation = previous().location;
  VectorT<StmtPtr> statements;

  while (!check(TokenType::RBrace) && !is_at_end()) {
    statements.emplace_back(statement());
  }

  consume(TokenType::RBrace, "Expected '}' after block");

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    BlockStmt{
      .statements = std::move(statements)
    }, 
  /*.location = */
    kLocation
  );
  // clang-format on
}

StmtPtr Parser::expression_statement() {
  const SourceLocation kLocation = peek().location;
  ExprPtr expr = expression();
  consume(TokenType::Semicolon, "Expected ';' after expression");

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    ExpressionStmt{
      .expression = std::move(expr)
    }, 
  /*.location = */
    kLocation
  );
  // clang-format on
}

StmtPtr Parser::var_decl_statement() {
  const Token kType = advance();
  const Token kName = consume(TokenType::Identifier, "Expected variable name");

  OptionalT<ExprPtr> initializer;
  if (match(TokenType::Assign)) {
    initializer = expression();
  }

  consume(TokenType::Semicolon, "Expected ';' after variable declaration");

  // clang-format off
  return make_unique<Statement>(
  /*.data = */
    VarDeclStmt{
      .type         = kType, 
      .name         = kName, 
      .initializer  = std::move(initializer)
    },
  /*.location = */
    kType.location
  );
  // clang-format on
}

ExprPtr Parser::expression() { return assignment(); }

ExprPtr Parser::assignment() {
  ExprPtr expr = equality();

  if (match(TokenType::Assign)) {
    const Token kEquals = previous();

    if (auto* ident = std::get_if<IdentifierExpr>(&expr->data)) {
      ExprPtr value = assignment();
      return make_unique<Expression>(
          AssignExpr{.name = ident->name, .value = std::move(value)},
          ident->name.location);
    }

    throw error(kEquals, "Invalid assignment target");
  }

  return expr;
}

ExprPtr Parser::equality() {
  ExprPtr expr = comparison();

  while (match(TokenType::Equal, TokenType::NotEqual)) {
    const Token kOp = previous();
    ExprPtr right = comparison();
    expr = make_unique<Expression>(
        BinaryExpr{
            .left = std::move(expr), .op = kOp, .right = std::move(right)},
        kOp.location);
  }

  return expr;
}

ExprPtr Parser::comparison() {
  ExprPtr expr = term();

  while (match(TokenType::Less, TokenType::Greater)) {
    const Token kOp = previous();
    ExprPtr right = term();
    expr = make_unique<Expression>(
        BinaryExpr{
            .left = std::move(expr), .op = kOp, .right = std::move(right)},
        kOp.location);
  }

  return expr;
}

ExprPtr Parser::term() {
  ExprPtr expr = unary();

  while (match(TokenType::Plus, TokenType::Minus)) {
    const Token kOp = previous();
    ExprPtr right = unary();
    expr = make_unique<Expression>(
        BinaryExpr{
            .left = std::move(expr), .op = kOp, .right = std::move(right)},
        kOp.location);
  }

  return expr;
}

ExprPtr Parser::unary() {
  if (match(TokenType::Not, TokenType::Minus)) {
    const Token kOp = previous();
    ExprPtr operand = unary();
    return make_unique<Expression>(
        UnaryExpr{.op = kOp, .operand = std::move(operand)}, kOp.location);
  }

  return primary();
}

ExprPtr Parser::primary() {
  if (match(TokenType::Number)) {
    const Token kToken = previous();
    return make_unique<Expression>(NumberExpr{.value = kToken},
                                   kToken.location);
  }

  if (match(TokenType::Identifier)) {
    const Token kName = previous();

    if (match(TokenType::LParen)) {
      return call(kName);
    }

    return make_unique<Expression>(IdentifierExpr{.name = kName},
                                   kName.location);
  }

  if (match(TokenType::LParen)) {
    const SourceLocation kLocation = previous().location;
    ExprPtr expr = expression();
    consume(TokenType::RParen, "Expected ')' after expression");
    return make_unique<Expression>(GroupingExpr{.expression = std::move(expr)},
                                   kLocation);
  }

  throw error(peek(), "Expected expression");
}

ExprPtr Parser::call(Token name) {
  VectorT<ExprPtr> arguments;

  if (not check(TokenType::RParen)) {
    do {
      arguments.emplace_back(expression());
    } while (match(TokenType::Comma));
  }

  consume(TokenType::RParen, "Expected ')' after arguments");

  return make_unique<Expression>(
      CallExpr{.callee = name, .arguments = std::move(arguments)},
      name.location);
}

bool Parser::is_at_end() const noexcept {
  return peek().type == TokenType::Eof;
}

Token Parser::peek() const noexcept { return tokens_[current_]; }

Token Parser::previous() const noexcept { return tokens_[current_ - 1]; }

Token Parser::advance() {
  if (not is_at_end()) {
    ++current_;
  }
  return previous();
}

bool Parser::check(TokenType type) const noexcept {
  if (is_at_end()) {
    return false;
  }
  return peek().type == type;
}

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) {
    return advance();
  }
  throw error(peek(), message);
}

bool Parser::is_type_token() const noexcept {
  return check(TokenType::Int) || check(TokenType::Void);
}

ParseError Parser::error(const Token& token, const std::string& message) {
  std::string full_message = "Line " + std::to_string(token.location.line) +
                             ", Col " + std::to_string(token.location.column) +
                             ": " + message;
  if (token.type != TokenType::Eof) {
    full_message += " (got '" + std::string(token.lexem) + "')";
  }
  return {full_message, token.location};
}

void Parser::synchronize() {
  advance();

  while (not is_at_end()) {
    if (previous().type == TokenType::Semicolon) {
      return;
    }

    switch (peek().type) {
      case TokenType::Int:
      case TokenType::Void:
      case TokenType::If:
      case TokenType::Return:
      case TokenType::Print:
        return;
      default:
        break;
    }
    advance();
  }
}

}  // namespace scy