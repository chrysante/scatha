#include "Sema/VTable.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

VTable const* VTable::inheritedVTable(RecordType const* type) const {
    auto itr = inheritanceMap.find(type);
    SC_EXPECT(itr != inheritanceMap.end());
    return itr->second.get();
}

std::unique_ptr<VTable> VTable::clone() const {
    utl::hashmap<RecordType const*, std::unique_ptr<VTable>> inheritanceClone;
    for (auto& [type, vtable]: inheritanceMap) {
        inheritanceClone[type] = vtable->clone();
    }
    return std::make_unique<VTable>(type(), std::move(inheritanceClone),
                                    _layout);
}

static QualType getPtrOrRefBase(Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::ReferenceType const& type) {
            return type.base();
        },
        [](sema::PointerType const& type) {
            return type.base();
        },
        [](sema::Type const&) ->QualType {
            SC_UNIMPLEMENTED();
        }
    }; // clang-format on
}

bool match(Function const& F, std::span<Type const* const> args) {
    SC_ASSERT(!F.argumentTypes().empty(), "");
    SC_ASSERT(!args.empty(), "");
    if (!ranges::equal(F.argumentTypes() | drop(1), args.subspan(1))) {
        return false;
    }
    auto objType = getPtrOrRefBase(F.argumentType(0));
    auto objArg = getPtrOrRefBase(args[0]);
    if (objType.mutability() != objArg.mutability() ||
        objType.bindMode() != objArg.bindMode())
    {
        return false;
    }
    return isDerivedFrom(cast<RecordType const*>(objArg.get()),
                         cast<RecordType const*>(objType.get()));
}

VTable::SearchResult<VTable const> VTable::findFunction(
    std::span<Type const* const> args) const {
    SC_EXPECT(!args.empty());
    for (auto [index, F]: layout() | enumerate) {
        if (match(*F, args)) {
            return { this, index };
        }
    }
    for (auto& [type, vtable]: inheritanceMap) {
        if (auto result = vtable->findFunction(args)) {
            return result;
        }
    }
    return {};
}

template <typename VT>
static utl::small_vector<VT*> sortedInheritedImpl(auto const& map) {
    auto result = map | ranges::views::values |
                  ranges::views::transform([](auto& p) { return p.get(); }) |
                  ranges::to<utl::small_vector<VT*>>;
    ranges::sort(result, ranges::less{}, &VTable::position);
    return result;
}

utl::small_vector<VTable*> VTable::sortedInheritedVTables() {
    return sortedInheritedImpl<VTable>(inheritanceMap);
}

utl::small_vector<VTable const*> VTable::sortedInheritedVTables() const {
    return sortedInheritedImpl<VTable const>(inheritanceMap);
}
