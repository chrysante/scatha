#ifndef SCATHA_SEMA_ANALYSIS_VTABLE_H_
#define SCATHA_SEMA_ANALYSIS_VTABLE_H_

#include <memory>
#include <span>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// List of functions in a vtable
class VTableLayout: private std::vector<Function*> {
public:
    using vector::begin;
    using vector::empty;
    using vector::end;
    using vector::size;
    using vector::operator[];
    using vector::push_back;
};

/// VTable for a struct or protocol type
class VTable {
public:
    explicit VTable(
        RecordType const* type,
        utl::hashmap<RecordType const*, std::unique_ptr<VTable>> inheritanceMap,
        VTableLayout layout):
        _type(type),
        inheritanceMap(std::move(inheritanceMap)),
        _layout(std::move(layout)) {}

    /// \Returns the corresponding record type
    RecordType const* type() const { return _type; }

    ///
    VTableLayout& layout() { return _layout; }

    /// \overload
    VTableLayout const& layout() const { return _layout; }

    /// \Returns the layout correspnding to \p type
    /// \Pre \p type must an ancestor type or a
    /// protocol this type or an ancestor type conforms to
    VTable* inheritedVTable(RecordType const* type) {
        return const_cast<VTable*>(std::as_const(*this).inheritedVTable(type));
    }

    /// \overload
    VTable const* inheritedVTable(RecordType const* type) const;

    ///
    std::unique_ptr<VTable> clone() const;

    ///
    size_t position() const { return pos; }

    ///
    void setPosition(size_t pos) { this->pos = pos; }

    template <typename VT>
    struct SearchResult {
        VT* vtable = nullptr;
        size_t index = 0;

        explicit operator bool() const { return vtable != nullptr; }

        operator SearchResult<std::add_const_t<VT>>() const {
            return { vtable, index };
        }
    };

    ///
    SearchResult<VTable> findFunction(std::span<Type const* const> args) {
        auto result = std::as_const(*this).findFunction(args);
        return { const_cast<VTable*>(result.vtable), result.index };
    }

    ///
    SearchResult<VTable const> findFunction(
        std::span<Type const* const> args) const;

    /// \Returns a sorted list of the inherited vtables
    utl::small_vector<VTable*> sortedInheritedVTables();

    /// \overload
    utl::small_vector<VTable const*> sortedInheritedVTables() const;

private:
    RecordType const* _type;
    utl::hashmap<RecordType const*, std::unique_ptr<VTable>> inheritanceMap;
    VTableLayout _layout;
    size_t pos = 0;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_VTABLE_H_
