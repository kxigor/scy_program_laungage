#include <include/config.hpp>
#include <include/type.hpp>

namespace scy {

namespace {
const UmapT<TypeKind, StringViewT> kTypesMap{{TypeKind::Int, "int"},
                                             {TypeKind::Void, "void"}};
}  // namespace

const StringViewT& TypeSpec::as_text() const { return kTypesMap.at(kind); }

}  // namespace scy