#include "Sema/SymbolID.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {

u64 SymbolID::hash() const {
    auto x = rawValue();
    x      = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x      = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x      = x ^ (x >> 31);
    return x;
}

std::ostream &operator<<(std::ostream &str, SymbolID id) {
    auto const flags = str.flags();
    str << std::hex << id.rawValue();
    str.flags(flags);
    return str;
}

SCATHA(API) std::string_view toString(SymbolCategory cat) {
    SC_ASSERT(std::has_single_bit(static_cast<uintmax_t>(cat)), "not a valid category");
    using enum SymbolCategory;
    switch (cat) {
    case Variable:
        return "Variable";
    case Namespace:
        return "Namespace";
    case OverloadSet:
        return "OverloadSet";
    case Function:
        return "Function";
    case ObjectType:
        return "ObjectType";
    case _count:
        SC_DEBUGFAIL();
    }
}

SCATHA(API) std::ostream &operator<<(std::ostream &str, SymbolCategory cat) { return str << toString(cat); }

} // namespace scatha::sema
