#pragma once

#include <cstdint>

#include "config.hpp"
#include "type.hpp"

namespace scy {

enum class SymbolKind : std::uint8_t { Variable, Function, Parameter };

struct VariableSymbol {
  StringViewT name;
  TypeSpec type;
};

struct ParameterSymbol {
  StringViewT name;
  TypeSpec type;
  PosT index;
};

struct FunctionSymbol {
  StringViewT name;
  TypeSpec return_type;
  VectorT<ParameterSymbol> params;
};

using SymbolData = VariantT<VariableSymbol, FunctionSymbol, ParameterSymbol>;

struct Symbol {
  SymbolKind kind;
  SymbolData data;
  SourceLocation location;
};

}  // namespace scy
