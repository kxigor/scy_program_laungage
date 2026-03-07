#pragma once

#include <vector>

#include "config.hpp"
#include "token.hpp"

namespace scy {

class Lexer {
 public:
  explicit Lexer(StringView source) : source_(source) {}

  std::vector<Token> tokenize();

 private:
  bool is_at_end() const;
  char peek() const;
  char peek_next() const;
  char advance();

  void skip_whitespace();

  Token next_token();
  Token make_token(TokenType type);
  Token error_token(StringView message);

  Token number();
  Token identifier();

  StringView source_;
  SourceLocation location_;

  std::size_t cursor_{0};
  std::size_t start_{0};
};

}  // namespace scy