#pragma once

#include <cstdint>

#include "config.hpp"

namespace scy {

enum class SemanticErrorKind : std::uint8_t {
  UndeclaredVariable,
  RedeclaredVariable,
  TypeMismatch,
  UndeclaredFunction,
  Other
};

struct SemanticError {
  SemanticErrorKind kind;
  StringT message;
  SourceLocation location;
};

}  // namespace scy
