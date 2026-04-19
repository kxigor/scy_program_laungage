#pragma once

#include "config.hpp"
#include "symbol.hpp"

namespace scy {

class Scope {
 public:
  explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

  bool define(StringViewT name, Symbol symbol);

  [[nodiscard]] Symbol* resolve(StringViewT name) noexcept;
  [[nodiscard]] const Symbol* resolve(StringViewT name) const noexcept;

  [[nodiscard]] Symbol* resolve_local(StringViewT name) noexcept;
  [[nodiscard]] const Symbol* resolve_local(StringViewT name) const noexcept;

  [[nodiscard]] Scope* parent() const noexcept { return parent_; }

  [[nodiscard]] const VectorT<UniquePtrT<Scope>>& children() const noexcept {
    return children_;
  }

  Scope* create_child();

 private:
  Scope* parent_;
  VectorT<UniquePtrT<Scope>> children_;
  UmapT<StringViewT, Symbol> symbols_;
};

}  // namespace scy
