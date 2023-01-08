#include "IR/Validate.h"

#include "IR/Module.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

void ir::setupInvariants(Context& ctx, Module& mod) {
    for (auto& function: mod.functions()) {
        setupInvariants(ctx, function);
    }
}

void link(ir::BasicBlock* a, ir::BasicBlock* b) {
    a->successors.push_back(b);
    b->predecessors.push_back(a);
};

static void insertReturn(Context& ctx, BasicBlock& bb) {
    bb.addInstruction(new Return(ctx));
}

void ir::setupInvariants(Context& ctx, Function& function) {
    for (auto& bb: function.basicBlocks()) {
        if (bb.instructions.empty() || !isa<TerminatorInst>(bb.instructions.back())) {
            insertReturn(ctx, bb);
            continue;
        }
        Instruction* inst = &bb.instructions.back();
        for (; inst != bb.instructions.end().to_address(); inst = inst->prev()) {
            if (!isa<TerminatorInst>(inst)) {
                break;
            }
        }
        auto* firstTerminator = cast<TerminatorInst*>(inst->next());
        using ListType = decltype(bb.instructions);
        bb.instructions.erase(ListType::iterator(firstTerminator->next()), bb.instructions.end());
        visit(*firstTerminator, utl::overload{ // clang-format off
            [&](ir::Goto& gt) {
                link(&bb, gt.target());
            },
            [&](ir::Branch& br) {
                link(&bb, br.thenTarget());
                link(&bb, br.elseTarget());
            },
            [&](ir::Return&) {
                /// Nothing to do here
            },
            [&](ir::TerminatorInst&) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
}
