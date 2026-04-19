#include <include/config.hpp>
#include <include/scope.hpp>
#include <include/symbol.hpp>
#include <utility>

namespace scy {

bool Scope::define(StringViewT name, Symbol symbol) {
  auto [_, inserted] = symbols_.emplace(name, std::move(symbol));
  return inserted;
}

Symbol* Scope::resolve(StringViewT name) noexcept {
  if (auto* sym = resolve_local(name)) {
    return sym;
  }
  if (parent_ != nullptr) {
    return parent_->resolve(name);
  }
  return nullptr;
}

const Symbol* Scope::resolve(StringViewT name) const noexcept {
  if (const auto* sym = resolve_local(name)) {
    return sym;
  }
  if (parent_ != nullptr) {
    return parent_->resolve(name);
  }
  return nullptr;
}

Symbol* Scope::resolve_local(StringViewT name) noexcept {
  auto it = symbols_.find(name);
  return it != symbols_.end() ? &it->second : nullptr;
}

const Symbol* Scope::resolve_local(StringViewT name) const noexcept {
  auto it = symbols_.find(name);
  return it != symbols_.end() ? &it->second : nullptr;
}

Scope* Scope::create_child() {
  children_.emplace_back(make_unique<Scope>(this));
  return children_.back().get();
}

}  // namespace scy
