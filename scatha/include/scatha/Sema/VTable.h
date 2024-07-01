#ifndef SCATHA_SEMA_ANALYSIS_VTABLE_H_
#define SCATHA_SEMA_ANALYSIS_VTABLE_H_

#include <memory>
#include <span>
#include <vector>

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

struct VTableElement {
    std::string name;
    FunctionType const* type;
    Function* function;
};

class VTable {
public:
    explicit VTable(
        RecordType const* type,
        utl::hashmap<RecordType const*, std::vector<VTableElement>> layouts):
        _type(type), layouts(std::move(layouts)) {}

    /// \Returns the corresponding record type
    RecordType const* type() const { return _type; }

    /// \Returns the layout correspnding to \p type
    /// \Pre \p type must be the corresponding type, an ancestor type or a
    /// protocol this type or an ancestor type conforms to
    std::span<VTableElement> layout(RecordType const* type) {
        auto L = std::as_const(*this).layout(type);
        return { const_cast<VTableElement*>(L.data()), L.size() };
    }

    /// \overload
    std::span<VTableElement const> layout(RecordType const* type) const {
        auto itr = layouts.find(type);
        SC_EXPECT(itr != layouts.end());
        return itr->second;
    }

private:
    RecordType const* _type;
    utl::hashmap<RecordType const*, std::vector<VTableElement>> layouts;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_VTABLE_H_
