#include "IR/Validate.h"

#include <iostream>
#include <string_view>

#include "Basic/Basic.h"
#include "IR/Module.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

/// ** Assertions **

#define CHECK(...) _doCheck(__VA_ARGS__, #__VA_ARGS__, __FUNCTION__, __LINE__)

static void _doCheck(bool             condition,
                     std::string_view conditionStr,
                     std::string_view functionName,
                     size_t           line)
{
    if (condition) {
        return;
    }
    std::cout << "IR Invariant <" << conditionStr << "> not satisfied.\n\t[In function \"" << functionName << "\" on line " << line << "]" << std::endl;
    SC_DEBUGBREAK();
}

void ir::assertInvariants(Context const& ctx, Module const& mod) {
    for (auto& function: mod.functions()) {
        assertInvariants(ctx, function);
    }
}

void ir::assertInvariants(Context const& ctx, Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        CHECK(bb.parent() == &function);
        assertInvariants(ctx, bb);
    }
}

void ir::assertInvariants(Context const& ctx, BasicBlock const& bb) {
    for (auto& inst: bb.instructions) {
        CHECK(inst.parent() == &bb);
        assertInvariants(ctx, inst);
    }
}

void ir::assertInvariants(Context const& ctx, Instruction const& inst) {
    for (auto* operand: inst.operands()) {
        auto opUsers = operand->users();
        CHECK(std::find(opUsers.begin(), opUsers.end(), &inst) != opUsers.end());
    }
    for (auto* user: inst.users()) {
        auto userOps = user->operands();
        CHECK(std::find(userOps.begin(), userOps.end(), &inst) != userOps.end());
    }
}

/// ** Setup **

void ir::setupInvariants(Context& ctx, Module& mod) {
    for (auto& function: mod.functions()) {
        setupInvariants(ctx, function);
    }
}

static void link(ir::BasicBlock* a, ir::BasicBlock* b) {
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
