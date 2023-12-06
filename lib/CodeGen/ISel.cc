#include "CodeGen/ISel.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/function_view.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISelCommon.h"
#include "CodeGen/Resolver.h"
#include "CodeGen/SDMatch.h"
#include "CodeGen/SelectionDAG.h"
#include "IR/CFG.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

template <>
struct Matcher<ir::Alloca>: MatcherBase {};

template <>
struct Matcher<ir::Load>: MatcherBase {
    void impl(ir::Load const& load,
              utl::function_view<mir::MemoryAddress(size_t)> addrCallback) {
        auto* dest = resolve(load);
        size_t numBytes = load.type()->size();
        size_t numWords = ::numWords(load);
        for (size_t i = 0; i < numWords; ++i, dest = dest->next()) {
            auto* inst = new mir::LoadInst(dest,
                                           addrCallback(i),
                                           sliceWidth(numBytes, i, numWords),
                                           load.metadata());
            emit(inst);
        }
    }

    // Load -> GEP
    SD_MATCH_CASE(ir::Load const& load, SelectionNode& node) {
        auto* gep = dyncast<ir::GetElementPointer const*>(load.address());
        if (!gep) {
            return false;
        }
        node.merge(DAG(gep));
        impl(load, [&](size_t i) { return computeGEP(gep, i * WordSize); });
        return true;
    }

    // Load
    SD_MATCH_CASE(ir::Load const& load, SelectionNode& node) {
        auto* baseAddr = resolveToRegister(*load.address(), load.metadata());
        impl(load, [&](size_t i) {
            return mir::MemoryAddress(baseAddr, i * WordSize);
        });
        return true;
    }
};

template <>
struct Matcher<ir::Store>: MatcherBase {
    void impl(ir::Store const& store,
              utl::function_view<mir::MemoryAddress(size_t)> addrCallback) {
        size_t numBytes = store.value()->type()->size();
        size_t numWords = utl::ceil_divide(numBytes, 8);
        /// We have these two cases for now because we can't iterate over
        /// "adjacent constants" so in the oversized case we resolve to register
        /// and store, in the simple case we store directly
        if (numWords <= 1) {
            auto* value = resolve(*store.value());
            auto* inst = new mir::StoreInst(addrCallback(0),
                                            value,
                                            numBytes,
                                            store.metadata());
            emit(inst);
        }
        else {
            auto* value = resolveToRegister(*store.value(), store.metadata());
            for (size_t i = 0; i < numWords; ++i, value = value->next()) {
                auto* inst =
                    new mir::StoreInst(addrCallback(i),
                                       value,
                                       sliceWidth(numBytes, i, numWords),
                                       store.metadata());
                emit(inst);
            }
        }
    }

    // Store -> GEP
    SD_MATCH_CASE(ir::Store const& store, SelectionNode& node) {
        auto* gep = dyncast<ir::GetElementPointer const*>(store.address());
        if (!gep) {
            return false;
        }
        node.merge(DAG(gep));
        impl(store, [&](size_t i) { return computeGEP(gep, i * WordSize); });
        return true;
    }

    // Store
    SD_MATCH_CASE(ir::Store const& store, SelectionNode& node) {
        auto* baseAddr = resolve(*store.address());
        impl(store, [&](size_t i) {
            return mir::MemoryAddress(baseAddr, i * WordSize);
        });
        return true;
    }
};

template <>
struct Matcher<ir::ConversionInst>: MatcherBase {};

template <>
struct Matcher<ir::CompareInst>: MatcherBase {};

template <>
struct Matcher<ir::UnaryArithmeticInst>: MatcherBase {};

template <>
struct Matcher<ir::ArithmeticInst>: MatcherBase {
    template <typename InstType>
    void doEmit(ir::ArithmeticInst const& inst, auto RHS) {
        auto* LHS = resolveToRegister(*inst.lhs(), inst.metadata());
        size_t size = inst.lhs()->type()->size();
        emit(new InstType(resolve(inst),
                          LHS,
                          RHS,
                          size,
                          inst.operation(),
                          inst.metadata()));
    }

    // Arithmetic -> Load -> GEP
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load) {
            return false;
        }
        auto* gep = dyncast<ir::GetElementPointer const*>(load->address());
        if (!gep) {
            return false;
        }
        node.merge(DAG(load));
        node.merge(DAG(gep));
        auto RHS = computeGEP(gep);
        doEmit<mir::LoadArithmeticInst>(inst, RHS);
        return true;
    }

    // Arithmetic -> Load
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load) {
            return false;
        }
        node.merge(DAG(load));
        auto RHS = mir::MemoryAddress(
            resolveToRegister(*load->address(), load->metadata()));
        doEmit<mir::LoadArithmeticInst>(inst, RHS);
        return true;
    }

    // Arithmetic (base case)
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* RHS = resolve(*inst.rhs());
        doEmit<mir::ValueArithmeticInst>(inst, RHS);
        return true;
    }
};

template <>
struct Matcher<ir::Goto>: MatcherBase {};

template <>
struct Matcher<ir::Branch>: MatcherBase {};

template <>
struct Matcher<ir::Return>: MatcherBase {};

template <>
struct Matcher<ir::Call>: MatcherBase {
    SD_MATCH_CASE(ir::Call const& call, SelectionNode& node) {
        utl::small_vector<mir::Value*, 16> args;
        for (auto* arg: call.arguments()) {
            auto* mirArg = resolve(*arg);
            size_t const numWords = ::numWords(*arg);
            for (size_t i = 0; i < numWords; ++i, mirArg = mirArg->next()) {
                args.push_back(mirArg);
            }
        }
        size_t const numDests = numWords(call);
        auto* dest = resolve(call);
        // clang-format off
        SC_MATCH (*call.function()) {
            [&](ir::Value const& func) {
                emit(new mir::CallInst(dest,
                                       numDests,
                                       resolve(func),
                                       std::move(args),
                                       call.metadata()));
            },
            [&](ir::ForeignFunction const& func) {
                emit(new mir::CallExtInst(dest,
                                          numDests,
                                          { .slot  = static_cast<uint32_t>(func.slot()),
                                            .index = static_cast<uint32_t>(func.index()) },
                                          std::move(args),
                                          call.metadata()));
            },
        }; // clang-format on
        return true;
    }
};

template <>
struct Matcher<ir::Phi>: MatcherBase {};

template <>
struct Matcher<ir::Select>: MatcherBase {};

template <>
struct Matcher<ir::GetElementPointer>: MatcherBase {
    SD_MATCH_CASE(ir::GetElementPointer const& gep, SelectionNode& node) {
        mir::MemoryAddress address = computeGEP(&gep);
        emit(new mir::LEAInst(resolve(gep), address, gep.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::ExtractValue>: MatcherBase {};

template <>
struct Matcher<ir::InsertValue>: MatcherBase {};

namespace {

struct ISelBlockCtx {
    SelectionDAG& DAG;

    /// List of instructions that will be populated for every node and then
    /// moved to that node
    List<mir::Instruction> instructions;

    ///
    Resolver resolver;

    /// Tuple of all instruction matchers
    std::tuple<
#define SC_INSTRUCTIONNODE_DEF(Inst, _) Matcher<ir::Inst>,
#include "IR/Lists.def"
        int>
        matchers;

    ISelBlockCtx(SelectionDAG& DAG,
                 mir::Context& ctx,
                 mir::Function& mirFn,
                 ValueMap& map):
        DAG(DAG), resolver(ctx, mirFn, map, [this](mir::Instruction* inst) {
            emit(inst);
        }) {
#define SC_INSTRUCTIONNODE_DEF(Inst, _)                                        \
    std::get<Matcher<ir::Inst>>(matchers).init(DAG, resolver);
#include "IR/Lists.def"
    }

    /// Runs the algorithm
    void run();

    /// Emits the instruction \p inst to the current node. Takes ownership of
    /// the pointer
    void emit(mir::Instruction* inst) { instructions.push_back(inst); }

    /// Sets the computed value and selected instruction to the node
    void finalizeNode(SelectionNode& node);

    /// Meant to be called for every node that shall be lowered
    void match(SelectionNode& node);
};

} // namespace

void cg::isel(SelectionDAG& DAG,
              mir::Context& ctx,
              mir::Function& mirFn,
              ValueMap& map) {

    auto& irBB = *DAG.basicBlock();
    generateGraphvizTmp(DAG, utl::strcat(irBB.name(), ".before"));
    ISelBlockCtx(DAG, ctx, mirFn, map).run();
    generateGraphvizTmp(DAG, utl::strcat(irBB.name(), ".after"));
}

static utl::small_vector<SelectionNode*> topsort(SelectionNode* root) {
    utl::small_vector<SelectionNode*> result;
    utl::hashset<SelectionNode const*> marked;
    auto visit = [&](auto visit, SelectionNode* node) {
        if (marked.contains(node)) {
            return;
        }
        for (auto* dep: node->dependencies()) {
            visit(visit, dep);
        }
        marked.insert(node);
        result.push_back(node);
    };
    visit(visit, root);
    ranges::reverse(result);
    return result;
}

void ISelBlockCtx::run() {
    for (auto* node: topsort(DAG.root())) {
        if (node->dependentValues().empty() && !DAG.hasSideEffects(node) &&
            !DAG.isOutput(node))
        {
            DAG.erase(node);
        }
        match(*node);
    }
}

static void reportUnmatched(ir::Instruction const& inst) {
    using namespace tfmt::modifiers;
    std::cout << tfmt::format(Red | Bold, "Error:")
              << " Failed to match instruction: ";
    ir::print(inst);
}

void ISelBlockCtx::match(SelectionNode& node) {
    auto& inst = *node.irInst();
    bool matched = visit(inst, [&]<typename Inst>(Inst const& inst) {
        return std::get<Matcher<Inst>>(matchers).match(inst, node);
    });
    if (matched) {
        finalizeNode(node);
    }
    else {
        reportUnmatched(inst);
    }
}

void ISelBlockCtx::finalizeNode(SelectionNode& node) {
    node.setMIR(resolver.resolve(*node.irInst()), std::move(instructions));
}
