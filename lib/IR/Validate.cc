#include "IR/Validate.h"

#include <iostream>
#include <string>
#include <string_view>

#include <utl/hashmap.hpp>

#include "Common/Base.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

/// ** Assertions **

#define CHECK(cond, msg) doCheck(cond, msg, #cond)

struct AssertContext {
    explicit AssertContext(ir::Context& ctx): ctx(ctx) {}

    void assertInvariants(Module const& mod);
    void assertInvariants(Function const& function);
    void assertInvariants(BasicBlock const& bb);
    void assertInvariants(Instruction const& inst);

    void assertSpecialInvariants(Value const&) { SC_UNREACHABLE(); }
    void assertSpecialInvariants(Instruction const&) {}
    void assertSpecialInvariants(Phi const&);
    void assertSpecialInvariants(Call const&);
    void assertSpecialInvariants(Branch const&);
    void assertSpecialInvariants(Load const&);
    void assertSpecialInvariants(Store const&);

    void uniqueName(Value const& value);

    void doCheck(bool condition,
                 std::string_view msg,
                 std::string_view conditionStr) const {
        if (condition) {
            return;
        }
        std::cout << "IR Invariant [" << conditionStr << "] not satisfied.\n";
        std::cout << "\t" << msg << "\n";
        if (currentFunction) {
            std::cout << "\tIn function " << currentFunction->name();
            if (currentBB) {
                std::cout << " in basic block " << currentBB->name();
            }
            std::cout << ":\n\n";
            ir::print(*currentFunction);
        }
        SC_DEBUGBREAK();
    }

    ir::Context& ctx;
    Function const* currentFunction = nullptr;
    BasicBlock const* currentBB     = nullptr;
    utl::hashmap<std::string, std::pair<Callable const*, Value const*>>
        nameValueMap;
    DominanceInfo::DomMap domMap;
};

void ir::assertInvariants(Context& ctx, Module const& mod) {
    AssertContext assertCtx(ctx);
    assertCtx.assertInvariants(mod);
}

void ir::assertInvariants(Context& ctx, Function const& function) {
    AssertContext assertCtx(ctx);
    assertCtx.assertInvariants(function);
}

void AssertContext::assertInvariants(Module const& mod) {
    for (auto* constant: mod.constants()) {
        CHECK(!isa<VoidType>(constant->type()), "Constants must have a type");
        CHECK(constant->data().size() == constant->type()->size(),
              "Constants data must be the same size as its type");
    }
    for (auto& function: mod) {
        assertInvariants(function);
    }
}

void AssertContext::assertInvariants(Function const& function) {
    currentFunction = &function;
    CHECK(!function.empty(), "Empty functions are invalid");
    /// Annoying that we have to `const_cast` here, but `DominanceInfo` exposes
    /// all references as mutable so we have no choice.
    domMap = DominanceInfo::computeDomSets(const_cast<Function&>(function));
    for (auto& bb: function) {
        CHECK(bb.parent() == &function,
              "Parent pointers must be setup correctly");
        assertInvariants(bb);
    }
    currentFunction = nullptr;
}

void AssertContext::assertInvariants(BasicBlock const& bb) {
    currentBB  = &bb;
    bool entry = true;
    CHECK(!bb.empty(),
          "Empty basic blocks are not well formed as they must end with a "
          "terminator");
    for (auto& inst: bb) {
        CHECK(inst.parent() == &bb, "Parent pointers must be setup correctly");
        assertInvariants(inst);
        if (entry && !isa<Phi>(inst)) {
            entry = false;
        }
        if (!entry) {
            CHECK(
                !isa<Phi>(inst),
                "Phi nodes may not appear after one non-phi node has appeared");
        }
        CHECK(!isa<TerminatorInst>(inst) ^ (&inst == &bb.back()),
              "The last instruction must be the one and only terminator of a "
              "basic block");
    }
    // clang-format off
    CHECK(bb.terminator() != nullptr, "Basic block must have a terminator");
    visit(*bb.terminator(), utl::overload{
        [&](Return const& ret) {
            auto const* const returnedType = ret.value()->type();
            CHECK(returnedType == bb.parent()->returnType(),
                  "Returned type must match return type of the function");
        },
        [](TerminatorInst const&) {},
    }); // clang-format on
    for (auto* pred: bb.predecessors()) {
        auto const predSucc = pred->successors();
        CHECK(std::find(predSucc.begin(), predSucc.end(), &bb) !=
                  predSucc.end(),
              "The predecessors of this basic block must have us listed as a "
              "successor");
    }
    for (auto* succ: bb.successors()) {
        auto const& succPred = succ->predecessors();
        CHECK(std::find(succPred.begin(), succPred.end(), &bb) !=
                  succPred.end(),
              "The successors of this basic block must have us listed as a "
              "predecessor");
    }
    currentBB = nullptr;
}

void AssertContext::assertInvariants(Instruction const& inst) {
    uniqueName(inst);
    for (auto* operand: inst.operands()) {
        CHECK(operand != nullptr, "Operands can't be null");
        auto opUsers = operand->users();
        CHECK(std::find(opUsers.begin(), opUsers.end(), &inst) != opUsers.end(),
              "Our operands must have listed us as their user");
        if (auto* opInst = dyncast<Instruction const*>(operand)) {
            CHECK(opInst->parent()->parent() == inst.parent()->parent(),
                  "If our operand is an instruction it must be in the same "
                  "function");
        }
        auto* instOp = dyncast<Instruction const*>(operand);
        if (instOp && !isa<Phi>(inst)) {
            auto& domSetOfInst = domMap.find(inst.parent())->second;
            CHECK(domSetOfInst.contains(instOp->parent()),
                  "If we use another instruction it must dominate us");
        }
    }
    for (auto* user: inst.users()) {
        auto userOps = user->operands();
        CHECK(std::find(userOps.begin(), userOps.end(), &inst) != userOps.end(),
              "Our users must actually use us");
        CHECK(user->parent()->parent() == inst.parent()->parent(),
              "If our user is an instruction it must be in the same function");
    }
    visit(inst, [this](auto& inst) { assertSpecialInvariants(inst); });
}

void AssertContext::assertSpecialInvariants(Phi const& phi) {
    auto const predsView = phi.parent()->predecessors();
    utl::hashset<BasicBlock const*> preds(predsView.begin(), predsView.end());
    CHECK(preds.size() == predsView.size(),
          "The incoming edges in the phi node must be unique");
    utl::hashset<BasicBlock const*> args(phi.incomingEdges().begin(),
                                         phi.incomingEdges().end());
    CHECK(preds == args,
          "We need an incoming edge in our phi node for exactly every incoming "
          "edge in the basic block");
    for (auto [pred, phiPred]:
         ranges::views::zip(phi.parent()->predecessors(), phi.incomingEdges()))
    {
        CHECK(pred == phiPred,
              "We also require that the predecessors to the phi node have the "
              "same order as the predecessors of the basic block.");
    }
}

void AssertContext::assertSpecialInvariants(Call const& call) {
    auto* func = call.function();
    CHECK(call.type() == func->returnType(), "Return type mismatch");
    CHECK(ranges::distance(func->parameters()) == call.arguments().size(),
          "We need an argument for every parameter");
    for (auto&& [param, arg]:
         ranges::views::zip(func->parameters(), call.arguments()))
    {
        CHECK(param.type() == arg->type(), "Argument type mismatch");
    }
}

void AssertContext::assertSpecialInvariants(Branch const& branch) {
    CHECK(branch.condition()->type() == ctx.integralType(1),
          "Condition must be type i1");
}

void AssertContext::assertSpecialInvariants(Load const& load) {
    CHECK(load.address()->type() == ctx.pointerType(),
          "Address must be of pointer type");
}

void AssertContext::assertSpecialInvariants(Store const& store) {
    CHECK(store.address()->type() == ctx.pointerType(),
          "Address must be of pointer type");
}

void AssertContext::uniqueName(Value const& value) {
    // TODO: This should actually check for globals, but we don't have a
    // `Global` type. Maybe add one?
    if (isa<Constant>(value)) {
        return;
    }
    if (value.name().empty()) {
        return;
    }
    // clang-format off
    Callable const* function = visit(value, utl::overload{
        [](Instruction const& inst) { return inst.parent()->parent(); },
        [](Parameter const& param) { return param.parent(); },
        [](BasicBlock const& bb) { return bb.parent(); },
        [](Value const& value) -> Function const* { SC_UNREACHABLE(); },
    }); // clang-format on
    auto const [itr, success] = nameValueMap.insert(
        { std::string(value.name()), { function, &value } });
    if (success) {
        return;
    }
    SC_ASSERT(!itr->first.empty(), "Name should not be empty at this stage");
    auto const [funcAddr, valueAddr] = itr->second;
    if (funcAddr != function) {
        return;
    }
    CHECK(valueAddr == &value,
          "A value with the same name must be the same value");
}

/// ** Setup **

void ir::setupInvariants(Context& ctx, Module& mod) {
    for (auto& function: mod) {
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
    bb.pushBack(new Return(ctx, ctx.undef(bb.parent()->returnType())));
}

void ir::setupInvariants(Context& ctx, Function& function) {
    for (auto& bb: function) {
        /// Erase everything after the last terminator.
        for (auto itr = bb.begin(); itr != bb.end(); ++itr) {
            if (isa<TerminatorInst>(*itr)) {
                bb.erase(std::next(itr), bb.end());
                break;
            }
        }
        /// If we don't have a terminator insert a return.
        if (bb.empty() || !isa<TerminatorInst>(bb.back())) {
            insertReturn(ctx, bb);
            continue;
        }
        auto* terminator = bb.terminator();
        // clang-format off
        visit(*terminator, utl::overload{
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
}
