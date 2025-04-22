#include "CodeGen/Passes.h"

#include "Common/Ranges.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

using namespace scatha;
using namespace cg;
using namespace mir;

namespace {

/// Worklist class for the algorithm. The worklist is implemented as a set to
/// make sure we don't have duplicates in the list
class Worklist {
public:
    explicit Worklist(Function& F):
        list(F.ssaRegisters() | TakeAddress |
             ranges::to<utl::hashset<SSARegister*>>) {}

    /// Add the instruction \p inst to the worklist
    void push(SSARegister* reg) { list.insert(reg); }

    /// Add all instructions that use the register \p reg to the worklist
    void pushUsers(Register* reg) {
        for (auto* user: reg->uses()) {
            for (auto* dest: user->destRegisters()) {
                push(cast<SSARegister*>(dest));
            }
        }
    }

    /// Pop an instruction off the worklist
    SSARegister* pop() {
        SC_EXPECT(!empty());
        auto* reg = *list.begin();
        list.erase(list.begin());
        return reg;
    }

    /// \Returns `true` if the list is empty
    bool empty() const { return list.empty(); }

private:
    utl::hashset<SSARegister*> list;
};

struct ISContext {
    Context& ctx;
    Function& F;

    Worklist worklist;

    ISContext(Context& ctx, Function& F): ctx(ctx), F(F), worklist(F) {}

    bool run();

    SSARegister* visitInst(Instruction& inst);

    SSARegister* visitImpl(Instruction&) { return nullptr; }
    SSARegister* visitImpl(CopyInst&);
    SSARegister* visitImpl(LEAInst&);
};

} // namespace

bool cg::instSimplify(Context& ctx, Function& F, CodegenOptions const&) {
    return ISContext(ctx, F).run();
}

bool ISContext::run() {
    bool modified = false;
    while (!worklist.empty()) {
        auto* reg = worklist.pop();
        auto* inst = reg->def();
        if (!inst) {
            continue;
        }
        auto* repl = visitInst(*inst);
        if (!repl) {
            continue;
        }
        modified = true;
        reg->replaceUsesWith(repl);
        inst->parent()->erase(inst);
        worklist.pushUsers(reg);
    }
    return modified;
}

SSARegister* ISContext::visitInst(Instruction& inst) {
    return visit(inst, [this](auto& inst) { return visitImpl(inst); });
}

SSARegister* ISContext::visitImpl(CopyInst& copy) {
    /// Register to register copies can be replaced with the source register
    if (auto* source = dyncast<SSARegister*>(copy.source())) {
        return source;
    }
    if (isa<UndefValue>(copy.source())) {
        return cast<SSARegister*>(copy.dest());
    }
    return nullptr;
}

SSARegister* ISContext::visitImpl(LEAInst& lea) {
    auto addr = lea.address();
    if (!addr.dynOffset() && addr.offsetTerm() == 0) {
        return dyncast<SSARegister*>(addr.baseAddress());
    }
    return nullptr;
}
