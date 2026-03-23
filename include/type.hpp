#pragma once

#include <cstdint>

#include "config.hpp"

namespace scy {

enum class TypeKind : std::uint8_t { Int, Void };

struct TypeSpec {
  [[nodiscard]] const StringViewT& as_text() const;

  TypeKind kind;
};

}  // namespace scy