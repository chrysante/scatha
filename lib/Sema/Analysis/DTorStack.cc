#include "Sema/DTorStack.h"

#include <optional>

#include "AST/AST.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

static std::optional<DestructorCall> makeDTorCall(Object* obj) {
    auto* type = obj->type().get();
    auto* structType = dyncast<StructureType const*>(type);
    if (!structType) {
        return std::nullopt;
    }
    using enum SpecialMemberFunction;
    auto* dtorSet = structType->specialMemberFunction(Delete);
    if (!dtorSet) {
        return std::nullopt;
    }
    SC_ASSERT(dtorSet->size() == 1, "Destructors cannot be overloaded");
    auto* function = dtorSet->front();
    return DestructorCall{ obj, function };
}

void DTorStack::push(Object* obj) {
    if (auto dtorCall = makeDTorCall(obj)) {
        dtorCalls.push(*dtorCall);
    }
}
