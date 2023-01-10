#include "IR/Validate.h"

#include <iostream>
#include <string>
#include <string_view>

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "IR/Module.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

/// ** Assertions **

#define CHECK(cond, msg) _doCheck(cond, msg, #cond, __FUNCTION__, __LINE__)

static void _doCheck(bool             condition,
                     std::string_view msg,
                     std::string_view conditionStr,
                     std::string_view functionName,
                     size_t           line)
{
    if (condition) {
        return;
    }
    std::cout << "IR Invariant [" << conditionStr << "] not satisfied.\n";
    std::cout << "\t\"" << msg << "\"\n";
    std::cout << "\tIn function \"" << functionName << "\" on line " << line << std::endl;
    SC_DEBUGBREAK();
}

struct AssertContext {
    explicit AssertContext(ir::Context const& ctx): ctx(ctx) {}
    
    void assertInvariants(Module const& mod);
    void assertInvariants(Function const& function);
    void assertInvariants(BasicBlock const& bb);
    void assertInvariants(Instruction const& inst);
    
    void uniqueName(Value const& value);
    
    ir::Context const& ctx;
    Function const* currentFunction = nullptr;
    BasicBlock const* currentBB = nullptr;
    utl::hashmap<std::string, std::pair<Function const*, Value const*>> nameValueMap;
};

void ir::assertInvariants(Context const& ctx, Module const& mod) {
    AssertContext assertCtx(ctx);
    assertCtx.assertInvariants(mod);
}

void AssertContext::assertInvariants(Module const& mod) {
    for (auto& function: mod.functions()) {
        currentFunction = &function;
        assertInvariants(function);
    }
}

void AssertContext::assertInvariants(Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        currentBB = &bb;
        CHECK(bb.parent() == &function, "Parent pointers must be setup correctly");
        assertInvariants(bb);
    }
}

void AssertContext::assertInvariants(BasicBlock const& bb) {
    for (auto& inst: bb.instructions) {
        CHECK(inst.parent() == &bb, "Parent pointers must be setup correctly");
        assertInvariants(inst);
    }
}

void AssertContext::assertInvariants(Instruction const& inst) {
    uniqueName(inst);
    for (auto* operand: inst.operands()) {
        uniqueName(*operand);
        auto opUsers = operand->users();
        CHECK(std::find(opUsers.begin(), opUsers.end(), &inst) != opUsers.end(),
              "The operands of instruction \"inst\" must have \"inst\" listed as their user");
        if (auto* opInst = dyncast<Instruction const*>(operand)) {
            CHECK(opInst->parent()->parent() == inst.parent()->parent(),
                  "If the operand of \"inst\" is an instruction it must be in the same function");
        }
    }
    for (auto* user: inst.users()) {
        uniqueName(*user);
        auto userOps = user->operands();
        CHECK(std::find(userOps.begin(), userOps.end(), &inst) != userOps.end(),
              "The users of a value must actually use that value");
        if (auto* userInst = dyncast<Instruction const*>(user)) {
            CHECK(userInst->parent()->parent() == inst.parent()->parent(),
                  "If the if the user of \"inst\" is an instruction it must be in the same function");
        }
    }
}

void AssertContext::uniqueName(Value const& value) {
    Function const* function = visit(value, utl::overload{
        [](Value const& value) -> Function const* { return nullptr; },
        [](Instruction const& inst) { return inst.parent()->parent(); },
        [](Parameter const& param) { return param.parent(); }
    });
    auto const [itr, success] = nameValueMap.insert({ std::string(value.name()), { function, &value } });
    if (success) { return; }
    auto& name = itr->first;
    if (name.empty()) { return; }
    auto const [funcAddr, valueAddr] = itr->second;
    if (funcAddr != function) { return; }
    CHECK(valueAddr == &value,
          "A value with the same name must be the same value");
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
