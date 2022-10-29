#include "Sema/SymbolID.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {

std::string_view toString(SymbolCategory cat) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(cat, {
        { SymbolCategory::Invalid,     "Invalid" },
        { SymbolCategory::Variable,    "Variable" },
        { SymbolCategory::Namespace,   "Namespace" },
        { SymbolCategory::OverloadSet, "OverloadSet" },
        { SymbolCategory::Function,    "Function" },
        { SymbolCategory::ObjectType,  "ObjectType" },
        { SymbolCategory::Anonymous,   "Anonymous" }
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, SymbolCategory cat) {
    return str << toString(cat);
}

u64 SymbolID::hash() const {
    u64 x = rawValue();
    x     = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x     = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x     = x ^ (x >> 31);
    return x;
}

std::ostream& operator<<(std::ostream& str, SymbolID id) {
    auto const flags = str.flags();
    str << std::hex << id.rawValue();
    str.flags(flags);
    return str;
}

} // namespace scatha::sema
