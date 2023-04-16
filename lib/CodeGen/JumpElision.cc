#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>

#include "MIR/CFG.h"

/// Jump elision tries to reorder the basic blocks in such a way that as many
/// edges as possible appear adjacent in the internal list of the function. Then
/// we can erase terminating jumps because control flow just *flows through* to
/// the next basic block.
///
/// This is archieved with a simple depth first search over the function.

using namespace scatha;
using namespace cg;

namespace {

struct JumpElimContext {
    JumpElimContext(mir::Function& F): F(F), L(std::move(F)) {}

    void run();

    void DFS(mir::BasicBlock* BB);

    mir::Function& F;
    CFGList<mir::Function, mir::BasicBlock> L;
    utl::hashset<mir::BasicBlock const*> visited;
};

} // namespace

void cg::elideJumps(mir::Function& F) { JumpElimContext(F).run(); }

void JumpElimContext::run() { DFS(&L.front()); }

void JumpElimContext::DFS(mir::BasicBlock* BB) {
    if (visited.contains(BB)) {
        return;
    }
    visited.insert(BB);
    L.extract(BB).release();
    F.pushBack(BB);
    auto* term = &BB->back();
    while (mir::isJump(term->instcode())) {
        auto* target = term->operandAt(0);
        /// Target could also be a function.
        if (auto* targetBB = dyncast<mir::BasicBlock*>(target)) {
            DFS(targetBB);
        }
        term = term->prev();
    }
    term = &BB->back();
    if (term->instcode() != mir::InstCode::Jump) {
        return;
    }
    if (term->operandAt(0) == BB->next()) {
        BB->erase(term);
    }
}
