#pragma once

#include "config.hpp"
#include "symbol.hpp"

namespace scy {

class Scope {
 public:
  explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

  /// Defines a symbol in this scope. Returns false if already defined here.
  bool define(StringViewT name, Symbol symbol);

  /// Resolves a symbol by name, walking up the scope chain.
  [[nodiscard]] Symbol* resolve(StringViewT name) noexcept;
  [[nodiscard]] const Symbol* resolve(StringViewT name) const noexcept;

  /// Resolves only in the current scope (no parent lookup).
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
