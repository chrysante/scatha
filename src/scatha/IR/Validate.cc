#include "IR/Validate.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/hashmap.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;
using namespace tfmt::modifiers;

SC_REGISTER_MODULE_PASS([](Context& ctx, Module& mod, auto&, auto&) {
    assertInvariants(ctx, mod);
    return false;
}, "validate", PassCategory::Other, {});

using GlobalEntityPtr = std::variant<Global const*, Type const*>;

InvariantException::InvariantException(utl::vstreammanip<> dynMessage):
    Exception("IR-invariant violated"), dynMessage(std::move(dynMessage)) {}

namespace {

struct AssertFnCtx {
    Context& ctx;
    Function const& function;
    BasicBlock const* currentBB = nullptr;
    utl::hashmap<std::string, Value const*> nameValueMap;
    DominanceInfo::DomMap domMap;

    AssertFnCtx(Context& ctx, Function const& F): ctx(ctx), function(F) {}

    void run();
    void assertInvariants(BasicBlock const& BB);
    void assertInvariants(Instruction const& inst);
    void assertInvariantsImpl(Instruction const&) {}
    void assertInvariantsImpl(Phi const&);
    void assertInvariantsImpl(Call const&);
    void assertInvariantsImpl(Branch const&);
    void assertInvariantsImpl(Load const&);
    void assertInvariantsImpl(Store const&);
    void assertInvariantsImpl(CompareInst const&);
    void assertInvariantsImpl(GetElementPointer const&);

    void checkUseDefDom(Instruction const& def, Instruction const& use);
    void uniqueLocalName(Value const& value);
    void check(bool condition, Value const& value, std::string_view msg) const;
};

struct AssertModCtx {
    AssertModCtx(Context& ctx, Module const& mod): ctx(ctx), mod(mod) {}

    void run();

    void uniqueGlobalName(Global const& value);
    void uniqueGlobalName(StructType const& type);
    void uniqueGlobalNameImpl(std::string_view name, GlobalEntityPtr);
    void check(bool condition, Value const& value, std::string_view msg) const;
    void check(bool condition, Type const& type, std::string_view msg) const;
    void check(bool condition, std::string_view msg,
               std::invocable<std::ostream&> auto print) const;

    Context& ctx;
    Module const& mod;
    utl::hashmap<std::string, GlobalEntityPtr> nameMap;
};

} // namespace

void ir::assertInvariants(Context& ctx, Module const& mod) {
    AssertModCtx assertCtx(ctx, mod);
    assertCtx.run();
}

static void doCheckGlobal(bool condition, Value const& value,
                          std::string_view msg);

static void assertGlobalInvariants(Context&, Global const& global) {
    for (auto* operand: global.operands()) {
        doCheckGlobal(operand, global, "Operands must not be null");
        doCheckGlobal(isa<Constant>(operand), global,
                      "Operands of globals must be constants");
    }
}

static void assertGlobalInvariants(Context& ctx, Function const& function) {
    AssertFnCtx assertCtx(ctx, function);
    assertCtx.run();
}

void ir::assertInvariants(Context& ctx, Global const& global) {
    visit(global, [&](auto& global) { assertGlobalInvariants(ctx, global); });
}

void AssertModCtx::run() {
    for (auto* type: mod.structTypes()) {
        uniqueGlobalName(*type);
    }
    for (auto& global: mod.globals()) {
        uniqueGlobalName(global);
        assertInvariants(ctx, global);
    }
    for (auto& function: mod) {
        uniqueGlobalName(function);
        assertInvariants(ctx, function);
    }
}

void AssertFnCtx::run() {
    check(!function.empty(), function, "Empty functions are invalid");
    /// Annoying that we have to `const_cast` here, but `DominanceInfo` exposes
    /// all references as mutable so we have no choice.
    domMap =
        DominanceInfo::computeDominatorSets(const_cast<Function&>(function));
    for (auto& BB: function) {
        check(BB.parent() == &function, BB,
              "Parent pointers must be setup correctly");
        assertInvariants(BB);
    }
}

void AssertFnCtx::assertInvariants(BasicBlock const& BB) {
    currentBB = &BB;
    check(
        !BB.empty(), BB,
        "Empty basic blocks are not well formed as they must end with a terminator");
    bool foundNonPhi = false;
    bool foundNonAlloca = false;
    for (auto& inst: BB) {
        check(inst.parent() == &BB, inst,
              "Parent pointers must be setup correctly");
        assertInvariants(inst);
        if (isa<Phi>(inst)) {
            check(
                !foundNonPhi, inst,
                "Phi nodes may not appear after one non-phi node has appeared");
            check(!BB.isEntry(), inst,
                  "Phi nodes may not appear in the entry block");
        }
        else {
            foundNonPhi = true;
        }
        if (isa<Alloca>(inst)) {
            check(
                !foundNonAlloca, inst,
                "Allocas may not appear after one non-alloca instruction has appeared");
            check(BB.isEntry(), inst,
                  "Allocas may only appear in the entry block");
        }
        else {
            foundNonAlloca = true;
        }
        check(
            isa<TerminatorInst>(inst) == (&inst == &BB.back()), inst,
            "The last instruction must be the one and only terminator of a basic block");
    }
    check(BB.terminator() != nullptr, BB, "Basic block must have a terminator");
    // clang-format off
    SC_MATCH (*BB.terminator()) {
        [&](Return const& ret) {
            auto const* returnedType = ret.value()->type();
            check(returnedType == BB.parent()->returnType(), ret,
                  "Returned type must match return type of the function");
        },
        [](TerminatorInst const&) {},
    }; // clang-format on
    for (auto* pred: BB.predecessors()) {
        auto predSucc = pred->successors();
        check(
            ranges::contains(predSucc, &BB), BB,
            "The predecessors of this basic block must have us listed as a successor");
    }
    for (auto* succ: BB.successors()) {
        auto const& succPred = succ->predecessors();
        check(
            ranges::contains(succPred, &BB), BB,
            "The successors of this basic block must have us listed as a predecessor");
    }
    currentBB = nullptr;
}

static BasicBlock::ConstIterator toInstItr(auto* p) {
    return BasicBlock::ConstIterator(p);
}

static BasicBlock::ConstIterator toInstItr(BasicBlock::ConstIterator p) {
    return p;
}

static auto BBRange(auto begin, auto end) {
    return ranges::make_subrange(toInstItr(begin), toInstItr(end)) |
           TakeAddress;
}

/// Asserts that \p def dominates \p use
void AssertFnCtx::checkUseDefDom(Instruction const& def,
                                 Instruction const& use) {
    check(&def != &use, use, "Instruction may not use itself");
    if (def.parent() == use.parent()) {
        check(ranges::contains(BBRange(def.parent()->begin(), &use), &def), use,
              "Defs must dominate uses");
    }
    else {
        auto itr = domMap.find(use.parent());
        SC_ASSERT(itr != domMap.end(), "");
        auto& domSet = itr->second;
        check(domSet.contains(def.parent()), use, "Defs must dominate uses");
    }
}

void AssertFnCtx::assertInvariants(Instruction const& inst) {
    uniqueLocalName(inst);
    if (!isa<VoidType>(inst.type())) {
        check(!inst.name().empty(), inst,
              "Non-void instructions must be named");
    }
    for (auto [index, operand]: inst.operands() | enumerate) {
        check(operand != nullptr, inst, "Operands can't be null");
        check(operand->countedUsers().contains(&inst), inst,
              "Our operands must have listed us as their user");
        if (auto* def = dyncast<Instruction const*>(operand)) {
            check(
                def->parentFunction() == &function, inst,
                "If our operand is an instruction it must be in the same function");
            if (isa<Phi>(inst)) {
                auto* pred = inst.parent()->predecessor(index);
                checkUseDefDom(*def, *pred->terminator());
            }
            else {
                checkUseDefDom(*def, inst);
            }
        }
    }
    for (auto* user: inst.users()) {
        check(ranges::contains(user->operands(), &inst), inst,
              "Our users must actually use us");
    }
    visit(inst, [this](auto& inst) { assertInvariantsImpl(inst); });
}

void AssertFnCtx::assertInvariantsImpl(Phi const& phi) {
    auto predsView = phi.parent()->predecessors();
    utl::hashset<BasicBlock const*> preds(predsView.begin(), predsView.end());
    check(preds.size() == predsView.size(), phi,
          "The incoming edges in the phi node must be unique");
    utl::hashset<BasicBlock const*> args(phi.incomingEdges().begin(),
                                         phi.incomingEdges().end());
    check(
        preds == args, phi,
        "We need an incoming edge in our phi node for exactly every incoming edge in the basic block");
    for (auto [pred, phiPred]:
         zip(phi.parent()->predecessors(), phi.incomingEdges()))
    {
        check(
            pred == phiPred, phi,
            "We also require that the predecessors to the phi node have the same order as the predecessors of the basic block.");
    }
}

void AssertFnCtx::assertInvariantsImpl(Call const& call) {
    if (auto* func = dyncast<Callable const*>(call.function())) {
        check(call.type() == func->returnType(), call, "Return type mismatch");
        check(ranges::distance(func->parameters()) ==
                  (ssize_t)call.arguments().size(),
              call, "We need an argument for every parameter");
        for (auto [param, arg]: zip(func->parameters(), call.arguments())) {
            check(param.type() == arg->type(), call, "Argument type mismatch");
        }
    }
    else {
        check(call.function()->type() == ctx.ptrType(), call,
              "Indirect calls must call ptr values");
    }
}

void AssertFnCtx::assertInvariantsImpl(Branch const& branch) {
    check(branch.condition()->type() == ctx.intType(1), branch,
          "Condition must be type i1");
    check(branch.thenTarget() != branch.elseTarget(), branch,
          "Branches must have distinct targets");
    check(branch.thenTarget()->parent() == &function, branch,
          "Branch instructions must branch within the same function");
    check(branch.elseTarget()->parent() == &function, branch,
          "Branch instructions must branch within the same function");
}

void AssertFnCtx::assertInvariantsImpl(Load const& load) {
    check(load.address()->type() == ctx.ptrType(), load,
          "Address must be of pointer type");
    check(load.type() != ctx.voidType(), load, "Cannot load void");
}

void AssertFnCtx::assertInvariantsImpl(Store const& store) {
    check(store.address()->type() == ctx.ptrType(), store,
          "Address must be of pointer type");
    if (auto* global = dyncast<GlobalVariable const*>(store.address())) {
        check(global->isMutable(), store, "Cannot write into global constant");
    }
}

void AssertFnCtx::assertInvariantsImpl(CompareInst const& cmp) {
    check(cmp.lhs()->type() == cmp.rhs()->type(), cmp,
          "Compare operands must have the same type");
    auto* type = cmp.lhs()->type();
    switch (cmp.mode()) {
    case CompareMode::Signed:
    case CompareMode::Unsigned:
        check(isa<IntegralType>(type) || isa<PointerType>(type), cmp,
              "Type must be integral");
        break;
    case CompareMode::Float:
        check(isa<FloatType>(type), cmp, "Type must be float");
        break;
    }
}

void AssertFnCtx::assertInvariantsImpl(GetElementPointer const& gep) {
    check(gep.basePointer()->type() == ctx.ptrType(), gep,
          "Base pointer must be of pointer type");
    Type const* accessedType = gep.inboundsType();
    for (size_t index: gep.memberIndices()) {
        auto* record = dyncast<RecordType const*>(accessedType);
        check(record, gep,
              "We can only have member indices if we are accessing a record");
        check(record->numElements() > index, gep,
              "GEP member index out of bounds");
        accessedType = record->memberAt(index).type;
    }
}

void AssertFnCtx::uniqueLocalName(Value const& value) {
    if (isa<Global>(value)) {
        return;
    }
    if (value.name().empty()) {
        check(value.type() == ctx.voidType(), value,
              "Anonymous values must be void");
        return;
    }
    auto [itr, success] =
        nameValueMap.insert({ std::string(value.name()), &value });
    if (success) {
        return;
    }
    SC_ASSERT(!itr->first.empty(), "Name should not be empty here");
    check(itr->second == &value, value,
          "A value with the same name must be the same value");
}

void AssertModCtx::uniqueGlobalName(Global const& value) {
    check(!value.name().empty(), value, "Globals must be named");
    uniqueGlobalNameImpl(value.name(), &value);
}

void AssertModCtx::uniqueGlobalName(StructType const& type) {
    check(!type.name().empty(), type, "Types must be named");
    uniqueGlobalNameImpl(type.name(), &type);
}

static auto format(GlobalEntityPtr entity) {
    return utl::streammanip([=](std::ostream& str) {
        auto fmtAddress = [&](void const* addr) {
            tfmt::FormatGuard guard(BrightGrey, str);
            str << " [" << addr << "]";
        };
        // clang-format off
        std::visit(csp::overload {
            [&](Global const* value) {
                printDecl(*value, str);
                fmtAddress(value);
            },
            [&](Type const* type) {
                str << *type;
                fmtAddress(type);
            }
        }, entity); // clang-format on
    });
}

void AssertModCtx::uniqueGlobalNameImpl(std::string_view name,
                                        GlobalEntityPtr entity) {
    auto [itr, success] = nameMap.insert({ std::string(name), entity });
    check(success, "Name already exists", [=](std::ostream& str) {
        str << "\tViolating definition: " << format(entity) << "\n";
        str << "\tExisting definition: " << format(itr->second) << "\n";
    });
}

static void doCheck(bool condition, std::string_view msg, Function const* F,
                    BasicBlock const* BB, Value const* value,
                    std::invocable<std::ostream&> auto valueDeclPrint) {
    if (condition) {
        return;
    }
    auto dynMsg = [=, msg = std::string(msg)](std::ostream& str) {
        str << tfmt::format(BGBrightRed | BrightWhite | Bold,
                            "IR-invariant violated\n");
        str << "\t" << msg << "\n";
        valueDeclPrint(str);
        if (F) {
            str << "\tIn function " << formatName(*F);
            if (BB && !isa<BasicBlock>(value)) {
                str << " in basic block " << formatName(*BB);
            }
            str << ":\n\n";
            print(*F, str);
        }
        else {
            str << "\n";
        }
    };
    if (std::getenv("SC_REPORT_INVARIANT_VIOLATIONS")) {
        dynMsg(std::cerr);
    }
    throw InvariantException(std::move(dynMsg));
}

void AssertFnCtx::check(bool condition, Value const& value,
                        std::string_view msg) const {
    doCheck(condition, msg, &function, currentBB, &value,
            [&](std::ostream& str) {
        str << "\t";
        printDecl(value, str);
        str << "\n";
    });
}

static void doCheckGlobal(bool condition, Value const& value,
                          std::string_view msg) {
    doCheck(condition, msg, nullptr, nullptr, &value, [&](std::ostream& str) {
        str << "\t";
        printDecl(value, str);
        str << "\n";
    });
}

void AssertModCtx::check(bool condition, Value const& value,
                         std::string_view msg) const {
    doCheckGlobal(condition, value, msg);
}

void AssertModCtx::check(bool condition, Type const& type,
                         std::string_view msg) const {
    doCheck(condition, msg, nullptr, nullptr, nullptr, [=](std::ostream& str) {
        str << "\t";
        str << type;
        str << "\n";
    });
}

void AssertModCtx::check(bool condition, std::string_view msg,
                         std::invocable<std::ostream&> auto print) const {
    doCheck(condition, msg, nullptr, nullptr, nullptr, std::move(print));
}
