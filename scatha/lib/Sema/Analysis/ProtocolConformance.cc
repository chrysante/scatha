#include "Sema/Analysis/ProtocolConformance.h"

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"
#include "Sema/VTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

static bool isDynRefOrPtr(Type const* type) {
    if (!type) {
        return false;
    }
    // clang-format off
    return SC_MATCH (*type){
        [](std::derived_from<PtrRefTypeBase> auto const& type) {
            return type.base().isDyn();
        }, 
        [](Type const&) { return false; }
    }; // clang-format on
}

static bool isVTableFunction(Function const* F) {
    return F->argumentCount() > 0 && isDynRefOrPtr(F->argumentType(0));
}

static std::unique_ptr<VTable> buildInheritedVTable(RecordType& recordType) {
    utl::hashmap<RecordType const*, std::unique_ptr<VTable>> inherited;
    for (auto [index, base]: recordType.baseTypes() | enumerate) {
        if (!base) {
            continue;
        }
        auto vtable = base->vtable()->clone();
        vtable->setPosition(index);
        inherited[base] = std::move(vtable);
    }
    return std::make_unique<VTable>(&recordType, std::move(inherited),
                                    VTableLayout{});
}

bool sema::analyzeProtocolConformance(AnalysisContext& ctx,
                                      RecordType& recordType) {
    auto vtable = buildInheritedVTable(recordType);
    for (auto* F:
         recordType.children() | Filter<Function> | filter(isVTableFunction))
    {
        auto locations = vtable->findFunction(*F);
        /// If this function doesn't override any inherited function, we create
        /// a new entry
        if (locations.empty()) {
            vtable->layout().push_back(F);
            continue;
        }
        /// Otherwise we set all overridden functions to `F`
        for (auto location: locations) {
            auto& layout = location.vtable->layout();
            auto* overridden = layout[location.index];
            if (overridden->returnType() &&
                overridden->returnType() != F->returnType())
            {
                ctx.issue<BadFuncDef>(
                    F->definition(),
                    BadFuncDef::OverridingFunctionWrongRetType);
            }
            layout[location.index] = F;
        }
    }
    recordType.setVTable(std::move(vtable));
    return true;
}
