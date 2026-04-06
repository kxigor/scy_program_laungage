#pragma once

#include "config.hpp"
#include "token.hpp"

namespace scy {

class Lexer {
 public:
  explicit Lexer(StringViewT source) : source_(source) {}

  VectorT<LocatedToken> tokenize();

 private:
  [[nodiscard]] bool is_at_end() const noexcept;
  [[nodiscard]] SymT peek() const noexcept;
  [[nodiscard]] SymT peek_next() const noexcept;
  SymT advance() noexcept;

  void skip_whitespace() noexcept;

  LocatedToken next_token();
  LocatedToken make_token(TokenType type);

  LocatedToken number();
  LocatedToken identifier();

  StringViewT source_;

  SourceLocation location_{.line = 1, .column = 1};

  PosT cursor_{0};
  PosT start_{0};
};

}  // namespace scy