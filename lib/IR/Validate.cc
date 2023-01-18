#include "IR/Validate.h"

#include <iostream>
#include <string>
#include <string_view>

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"
#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;

/// ** Assertions **

#define CHECK(cond, msg) _doCheck(cond, msg, #cond, __FUNCTION__, __LINE__)

static void _doCheck(
    bool condition, std::string_view msg, std::string_view conditionStr, std::string_view functionName, size_t line) {
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

    void assertSpecialInvariants(Value const&) { SC_UNREACHABLE(); }
    void assertSpecialInvariants(Instruction const&) {}
    void assertSpecialInvariants(Phi const&);

    void uniqueName(Value const& value);

    ir::Context const& ctx;
    Function const* currentFunction = nullptr;
    BasicBlock const* currentBB     = nullptr;
    utl::hashmap<std::string, std::pair<Function const*, Value const*>> nameValueMap;
};

void ir::assertInvariants(Context const& ctx, Module const& mod) {
    AssertContext assertCtx(ctx);
    assertCtx.assertInvariants(mod);
}

void ir::assertInvariants(Context const& ctx, Function const& function) {
    AssertContext assertCtx(ctx);
    assertCtx.assertInvariants(function);
}

void AssertContext::assertInvariants(Module const& mod) {
    for (auto& function: mod.functions()) {
        assertInvariants(function);
    }
}

void AssertContext::assertInvariants(Function const& function) {
    currentFunction = &function;
    for (auto& bb: function.basicBlocks()) {
        CHECK(bb.parent() == &function, "Parent pointers must be setup correctly");
        assertInvariants(bb);
    }
    currentFunction = nullptr;
}

void AssertContext::assertInvariants(BasicBlock const& bb) {
    currentBB  = &bb;
    bool entry = true;
    CHECK(!bb.empty(), "Empty basic blocks are not well formed as they must end with a terminator");
    for (auto& inst: bb) {
        CHECK(inst.parent() == &bb, "Parent pointers must be setup correctly");
        assertInvariants(inst);
        if (entry && !isa<Phi>(inst)) {
            entry = false;
        }
        if (!entry) {
            CHECK(!isa<Phi>(inst), "Phi nodes may not appear after one non-phi node has appeared");
        }
        CHECK(!isa<TerminatorInst>(inst) ^ (&inst == &bb.back()),
              "The last instruction must be the one and only terminator of a basic block");
    }
    for (auto* pred: bb.predecessors()) {
        auto const predSucc = pred->successors();
        CHECK(std::find(predSucc.begin(), predSucc.end(), &bb) != predSucc.end(),
              "The predecessors of this basic block must have us listed as a successor");
    }
    for (auto* succ: bb.successors()) {
        auto const& succPred = succ->predecessors();
        CHECK(std::find(succPred.begin(), succPred.end(), &bb) != succPred.end(),
              "The successors of this basic block must have us listed as a predecessor");
    }
    currentBB = nullptr;
}

void AssertContext::assertInvariants(Instruction const& inst) {
    uniqueName(inst);
    for (auto* operand: inst.operands()) {
        uniqueName(*operand);
        auto opUsers = operand->users();
        CHECK(std::find(opUsers.begin(), opUsers.end(), &inst) != opUsers.end(),
              "Our operands must have listed us as their user");
        if (auto* opInst = dyncast<Instruction const*>(operand)) {
            CHECK(opInst->parent()->parent() == inst.parent()->parent(),
                  "If our operand is an instruction it must be in the same function");
        }
    }
    for (auto* user: inst.users()) {
        uniqueName(*user);
        auto userOps = user->operands();
        CHECK(std::find(userOps.begin(), userOps.end(), &inst) != userOps.end(), "Our users must actually use us");
        if (auto* userInst = dyncast<Instruction const*>(user)) {
            CHECK(userInst->parent()->parent() == inst.parent()->parent(),
                  "If our user is an instruction it must be in the same function");
        }
    }
    visit(inst, [this](auto& inst) { assertSpecialInvariants(inst); });
}

void AssertContext::assertSpecialInvariants(Phi const& phi) {
    auto const predsView = phi.parent()->predecessors();
    utl::hashset<BasicBlock const*> preds(predsView.begin(), predsView.end());
    CHECK(preds.size() == predsView.size(), "The incoming edges in the phi node must be unique");
    utl::hashset<BasicBlock const*> args(phi.incomingEdges().begin(), phi.incomingEdges().end());
    CHECK(preds == args, "We need an incoming edge in our phi node for exactly every incoming edge in the basic block");
}

void AssertContext::uniqueName(Value const& value) {
    // clang-format off
    Function const* function = visit(value, utl::overload{
        [](Value const& value) -> Function const* { return nullptr; },
        [](Instruction const& inst) { return inst.parent()->parent(); },
        [](Parameter const& param) { return param.parent(); }
    }); // clang-format on
    auto const [itr, success] = nameValueMap.insert({ std::string(value.name()), { function, &value } });
    if (success) {
        return;
    }
    auto& name = itr->first;
    if (name.empty()) {
        return;
    }
    auto const [funcAddr, valueAddr] = itr->second;
    if (funcAddr != function) {
        return;
    }
    CHECK(valueAddr == &value, "A value with the same name must be the same value");
}

/// ** Setup **

void ir::setupInvariants(Context& ctx, Module& mod) {
    for (auto& function: mod.functions()) {
        setupInvariants(ctx, function);
    }
}

static void link(ir::BasicBlock* a, ir::BasicBlock* b) {
    if (b->isPredecessor(a)) {
        return;
    }
    b->addPredecessor(a);
};

static void insertReturn(Context& ctx, BasicBlock& bb) {
    bb.pushBack(new Return(ctx));
}

void ir::setupInvariants(Context& ctx, Function& function) {
    for (auto& bb: function.basicBlocks()) {
        if (bb.empty() || !isa<TerminatorInst>(bb.back())) {
            insertReturn(ctx, bb);
            continue;
        }
        Instruction* inst = &bb.back();
        for (; inst != bb.end().to_address(); inst = inst->prev()) {
            if (!isa<TerminatorInst>(inst)) {
                break;
            }
        }
        auto* firstTerminator = cast<TerminatorInst*>(inst->next());
        bb.erase(BasicBlock::Iterator(firstTerminator->next()), bb.end());
        // clang-format off
        visit(*firstTerminator, utl::overload{
            [&](ir::Goto& gt) {
                link(&bb, gt.target());
            },
            [&](ir::Branch& br) {
                link(&bb, br.thenTarget());
                link(&bb, br.elseTarget());
            },
            [&](ir::Return&) {},
            [&](ir::TerminatorInst&) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
    auto& basicBlocks = function.basicBlocks();
    /// We erase blocks that have no predecessors thus cannot be reached.
    for (auto itr = basicBlocks.begin(); itr != basicBlocks.end();) {
        if (!itr->isEntry() && itr->predecessors().empty()) {
            for (auto* succ: itr->successors()) {
                succ->removePredecessor(itr.to_address());
            }
            itr = basicBlocks.erase(itr);
        }
        else {
            ++itr;
        }
    }
}
