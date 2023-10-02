#include "Sema/Analysis/DTorStack.h"

#include <optional>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

static std::optional<DestructorCall> makeDTorCall(Object* obj) {
    auto* type = obj->type();
    auto* compType = dyncast<CompoundType const*>(type);
    if (!compType) {
        return std::nullopt;
    }
    using enum SpecialLifetimeFunction;
    auto* dtor = compType->specialLifetimeFunction(Destructor);
    if (!dtor) {
        return std::nullopt;
    }
    return DestructorCall{ obj, dtor };
}

void DTorStack::push(Object* obj) {
    if (auto dtorCall = makeDTorCall(obj)) {
        push(*dtorCall);
    }
}

void DTorStack::push(DestructorCall dtorCall) { dtorCalls.push(dtorCall); }
