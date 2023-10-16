#include "IR/CFG/GlobalVar.h"

#include "IR/CFG/Constant.h"
#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

GlobalVarBase::GlobalVarBase(NodeType nodeType,
                             ir::Context& ctx,
                             Constant* init,
                             std::string name):
    Global(nodeType, ctx.ptrType(), std::move(name), { init }) {}

Constant const* GlobalVarBase::initializer() const {
    return cast<Constant const*>(operandAt(0));
}

PointerType const* GlobalVarBase::type() const {
    return cast<PointerType const*>(Value::type());
}

GlobalVar::GlobalVar(ir::Context& ctx, Constant* init, std::string name):
    GlobalVarBase(NodeType::GlobalVar, ctx, init, std::move(name)) {}

GlobalConst::GlobalConst(ir::Context& ctx, Constant* init, std::string name):
    GlobalVarBase(NodeType::GlobalConst, ctx, init, std::move(name)) {}
