#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include "ast.hpp"
#include "config.hpp"
#include "token.hpp"

namespace scy {

class ParseError : public std::runtime_error {
 public:
  ParseError(const std::string& message, SourceLocation location)
      : std::runtime_error(message), location_(location) {}

  [[nodiscard]] SourceLocation location() const noexcept { return location_; }

 private:
  SourceLocation location_;
};

class Parser {
 public:
  explicit Parser(VectorT<LocatedToken> tokens) : tokens_(std::move(tokens)) {}

  Program parse();

  [[nodiscard]] const VectorT<ParseError>& errors() const noexcept {
    return errors_;
  }

  [[nodiscard]] bool has_errors() const noexcept { return not errors_.empty(); }

 private:
  DeclPtr declaration();
  DeclPtr function_declaration(LocatedToken type, LocatedToken name);
  DeclPtr variable_declaration(LocatedToken type, LocatedToken name);

  StmtPtr statement();
  StmtPtr if_statement();
  StmtPtr return_statement();
  StmtPtr block_statement();
  StmtPtr expression_statement();
  StmtPtr var_decl_statement();

  ExprPtr expression();
  ExprPtr assignment();
  ExprPtr equality();
  ExprPtr comparison();
  ExprPtr term();
  ExprPtr unary();
  ExprPtr primary();
  ExprPtr call(LocatedToken name);

  [[nodiscard]] bool is_at_end() const noexcept;
  [[nodiscard]] LocatedToken peek() const noexcept;
  [[nodiscard]] LocatedToken previous() const noexcept;
  LocatedToken advance();

  [[nodiscard]] bool check(TokenType type) const noexcept;
  bool match(TokenType type);

  template <typename... Types>
  bool match(TokenType first, Types... rest) {
    if (match(first)) {
      return true;
    }
    return match(rest...);
  }

  LocatedToken consume(TokenType type, const std::string& message);

  [[nodiscard]] bool is_type_token() const noexcept;

  static ParseError error(const LocatedToken& token,
                          const std::string& message);
  void synchronize();

  VectorT<LocatedToken> tokens_;
  PosT current_{0};
  VectorT<ParseError> errors_;
};

}  // namespace scy
