#include "IR/CFG/GlobalVariable.h"

#include "IR/Context.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

GlobalVariable::GlobalVariable(Context& ctx, Mutability mut, Constant* init,
                               std::string name):
    Global(NodeType::GlobalVariable, ctx.ptrType(), std::move(name),
           ValueArray{ init }),
    mut(mut) {
    setInitializer(init);
}

Constant const* GlobalVariable::initializer() const {
    return cast<Constant const*>(operandAt(0));
}

void GlobalVariable::setInitializer(Constant* init) {
    setOperand(0, init);
    if (!init) {
        return;
    }
    auto* type = init->type();
    allocatePointerInfo({ .align = type->align(),
                          .validSize = type->size(),
                          .provenance = this,
                          .staticProvenanceOffset = 0,
                          .guaranteedNotNull = true });
}

PointerType const* GlobalVariable::type() const {
    return cast<PointerType const*>(Value::type());
}
