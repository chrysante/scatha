#ifndef SCATHA_IR_SYMBOLID_H_
#define SCATHA_IR_SYMBOLID_H_

#include <functional>

#include "Basic/Basic.h"

namespace scatha::ir {
	
enum class SymbolKind {
    type, function, variable
};

class SymbolID {
public:
    explicit SymbolID(u64 rawValue, SymbolKind kind): raw(rawValue), _kind(kind) {}
    
    u64 rawValue() const { return raw; }
    
    SymbolKind kind() const { return _kind; }
    
private:
    u64 raw: 60;
    SymbolKind _kind: 4;
};
	
inline bool operator==(SymbolID const& rhs, SymbolID const& lhs) {
    bool const result = rhs.rawValue() == lhs.rawValue();
    SC_ASSERT(!result || rhs.kind() == lhs.kind(), "If IDs are the same the kinds must be the same too.");
    return result;
}

} // namespace scatha::ir

template <>
struct std::hash<scatha::ir::SymbolID> {
    std::size_t operator()(scatha::ir::SymbolID const& symbolID) const { return std::hash<scatha::u64>{}(symbolID.rawValue()); }
};

#endif // SCATHA_IR_SYMBOLID_H_

