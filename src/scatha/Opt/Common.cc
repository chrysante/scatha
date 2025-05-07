#include "Opt/Common.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/Graph.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_FUNCTION_PASS(opt::splitCriticalEdges, "splitcriticaledges",
                          PassCategory::Simplification, {});

bool opt::preceeds(Instruction const* a, Instruction const* b) {
    SC_ASSERT(a->parent() == b->parent(),
              "a and b must be in the same basic block");
    auto* BB = a->parent();
    for (auto itr = BasicBlock::ConstIterator(a); itr != BB->end(); ++itr) {
        if (itr.to_address() == b) {
            return true;
        }
    }
    return false;
}

void opt::moveAllocas(BasicBlock* from, BasicBlock* to) {
    if (from == to) {
        return;
    }
    auto insertPoint =
        ranges::find_if(*to, [](auto& inst) { return !isa<Alloca>(inst); });
    for (auto itr = from->begin(); itr != from->end();) {
        auto& inst = *itr;
        if (!isa<Alloca>(inst)) {
            break;
        }
        ++itr;
        from->extract(&inst).release();
        to->insert(insertPoint, &inst);
    }
}

static bool cmpEqImpl(Phi const* lhs, auto rhs) {
    if (lhs->argumentCount() != ranges::size(rhs)) {
        return false;
    }
    auto lhsArgs = ranges::views::common(lhs->arguments());
    utl::hashset<ConstPhiMapping> lhsSet(lhsArgs.begin(), lhsArgs.end());
    auto rhsCommon = ranges::views::common(rhs);
    utl::hashset<ConstPhiMapping> rhsSet(rhsCommon.begin(), rhsCommon.end());
    return lhsSet == rhsSet;
}

bool opt::compareEqual(Phi const* lhs, std::span<ConstPhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

bool opt::compareEqual(Phi const* lhs, std::span<PhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

BasicBlock* opt::splitEdge(std::string name, Context& ctx, BasicBlock* from,
                           BasicBlock* to) {
    auto* tmp = new BasicBlock(ctx, std::move(name));
    auto* function = from->parent();
    function->insert(to, tmp);
    tmp->pushBack(new Goto(ctx, to));
    from->terminator()->updateTarget(to, tmp);
    to->updatePredecessor(from, tmp);
    tmp->addPredecessor(from);
    return tmp;
}

BasicBlock* opt::splitEdge(Context& ctx, BasicBlock* from, BasicBlock* to) {
    return splitEdge("tmp", ctx, from, to);
}

bool opt::splitCriticalEdges(Context& ctx, Function& function) {
    struct DFS {
        Context& ctx;
        utl::hashset<BasicBlock*> visited = {};
        bool modified = false;

        void search(BasicBlock* BB) {
            if (!visited.insert(BB).second) {
                return;
            }
            for (auto* succ: BB->successors()) {
                if (isCriticalEdge(BB, succ)) {
                    splitEdge(ctx, BB, succ);
                    modified = true;
                }
                search(succ);
            }
        }
    };
    DFS dfs{ ctx };
    dfs.search(&function.entry());
    if (dfs.modified) {
        function.invalidateCFGInfo();
    }
    return dfs.modified;
}

BasicBlock* opt::addJoiningPredecessor(Context& ctx, BasicBlock* header,
                                       std::span<BasicBlock* const> preds,
                                       std::string name) {
    // clang-format off
    SC_ASSERT(ranges::all_of(preds, [&](auto* pred) {
        return ranges::contains(pred->successors(), header);
    }), "preds must be predecessors of BB"); // clang-format on
    auto& function = *header->parent();
    auto* preheader = new BasicBlock(ctx, name);
    function.insert(header, preheader);
    for (auto& phi: header->phiNodes()) {
        utl::small_vector<PhiMapping> args;
        for (auto* pred: preds) {
            args.push_back({ pred, phi.operandOf(pred) });
        }
        auto* value = [&]() -> Value* {
            if (args.empty()) {
                return ctx.undef(phi.type());
            }
            if (args.size() == 1) {
                return args.front().value;
            }
            auto* preheaderPhi = new Phi(args, std::string(phi.name()));
            preheader->pushBack(preheaderPhi);
            return preheaderPhi;
        }();
        phi.addArgument(preheader, value);
    }
    for (auto* pred: preds) {
        pred->terminator()->updateTarget(header, preheader);
        header->removePredecessor(pred);
    }
    preheader->setPredecessors(preds);
    preheader->pushBack(new Goto(ctx, header));
    header->addPredecessor(preheader);
    return preheader;
}

static bool callCasSideEffects(Call const* call) {
    auto* function = dyncast<Callable const*>(call->function());
    if (!function) {
        return true;
    }
    return !function->hasAttribute(FunctionAttribute::Memory_WriteNone);
}

bool opt::hasSideEffects(Instruction const* inst) {
    if (auto* call = dyncast<Call const*>(inst)) {
        return callCasSideEffects(call);
    }
    if (isa<Store>(inst)) {
        return true;
    }
    return false;
}

bool opt::isMemcpy(Instruction const* inst) {
    auto* call = dyncast<Call const*>(inst);
    if (!call || !call->function()) {
        return false;
    }
    return call->function()->name() == "__builtin_memcpy";
}

bool opt::isConstSizeMemcpy(Instruction const* inst) {
    if (!isMemcpy(inst)) {
        return false;
    }
    auto* call = cast<Call const*>(inst);
    return isa<IntegralConstant>(call->argumentAt(1)) &&
           isa<IntegralConstant>(call->argumentAt(3));
}

Value const* opt::memcpyDest(Instruction const* call) {
    SC_ASSERT(isMemcpy(call), "Invalid");
    return cast<Call const*>(call)->argumentAt(0);
}

Value const* opt::memcpySource(Instruction const* call) {
    SC_ASSERT(isMemcpy(call), "Invalid");
    return cast<Call const*>(call)->argumentAt(2);
}

size_t opt::memcpySize(Instruction const* inst) {
    SC_ASSERT(isConstSizeMemcpy(inst), "Invalid");
    auto* call = cast<Call const*>(inst);
    auto* size = cast<IntegralConstant const*>(call->argumentAt(1));
    return size->value().to<size_t>();
}

void opt::setMemcpyDest(Instruction* call, Value* dest) {
    SC_ASSERT(isMemcpy(call), "Invalid");
    cast<Call*>(call)->setArgument(0, dest);
}

void opt::setMemcpySource(Instruction* call, Value* source) {
    SC_ASSERT(isMemcpy(call), "Invalid");
    cast<Call*>(call)->setArgument(2, source);
}

bool opt::isMemset(ir::Instruction const* inst) {
    auto* call = dyncast<Call const*>(inst);
    if (!call || !call->function()) {
        return false;
    }
    return call->function()->name() == "__builtin_memset";
}

bool opt::isConstMemset(ir::Instruction const* inst) {
    if (!isMemset(inst)) {
        return false;
    }
    auto* call = cast<Call const*>(inst);
    return isa<IntegralConstant>(call->argumentAt(1)) &&
           isa<IntegralConstant>(call->argumentAt(2));
}

bool opt::isConstZeroMemset(ir::Instruction const* inst) {
    return isConstMemset(inst) && memsetConstValue(inst) == 0;
}

ir::Value const* opt::memsetDest(ir::Instruction const* call) {
    SC_ASSERT(isMemset(call), "Invalid");
    return cast<Call const*>(call)->argumentAt(0);
}

void opt::setMemsetDest(ir::Instruction* call, ir::Value* dest) {
    SC_ASSERT(isMemset(call), "Invalid");
    cast<Call*>(call)->setArgument(0, dest);
}

size_t opt::memsetSize(ir::Instruction const* inst) {
    SC_ASSERT(isConstMemset(inst), "Invalid");
    auto* call = cast<Call const*>(inst);
    auto* size = cast<IntegralConstant const*>(call->argumentAt(1));
    return size->value().to<size_t>();
}

ir::Value const* opt::memsetValue(ir::Instruction const* inst) {
    SC_ASSERT(isMemset(inst), "Invalid");
    auto* call = cast<Call const*>(inst);
    return call->argumentAt(2);
}

int64_t opt::memsetConstValue(ir::Instruction const* inst) {
    SC_ASSERT(isConstMemset(inst), "Invalid");
    auto* size = cast<IntegralConstant const*>(memsetValue(inst));
    return size->value().to<int64_t>();
}

static std::string_view toName(svm::Builtin builtin) {
    switch (builtin) {
#define SVM_BUILTIN_DEF(Name, ...)                                             \
    case svm::Builtin::Name:                                                   \
        return "__builtin_" #Name;
#include <svm/Builtin.def.h>
    }
    SC_UNREACHABLE();
}

bool opt::isBuiltinCall(ir::Value const* value, svm::Builtin builtin) {
    auto* call = dyncast<Call const*>(value);
    if (!call || !call->function()) {
        return false;
    }
    auto* F = call->function();
    return isa<ForeignFunction>(F) && F->name() == toName(builtin);
}
