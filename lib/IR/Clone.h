#ifndef SCATHA_IR_CLONE_H_
#define SCATHA_IR_CLONE_H_

#include <concepts>

#include "Common/Base.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class Context;

SCATHA_TESTAPI UniquePtr<Instruction> clone(Context& context,
                                            Instruction* inst);

template <std::derived_from<Instruction> Inst>
SCATHA_TESTAPI UniquePtr<Inst> clone(Context& context, Inst* inst) {
    return uniquePtrCast<Inst>(clone(context, static_cast<Instruction*>(inst)));
}

SCATHA_TESTAPI UniquePtr<BasicBlock> clone(Context& context, BasicBlock* BB);

SCATHA_TESTAPI UniquePtr<Function> clone(Context& context, Function* function);

} // namespace scatha::ir

#endif // SCATHA_IR_CLONE_H_
