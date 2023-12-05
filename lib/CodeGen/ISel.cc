#include "CodeGen/ISel.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
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
    SD_MATCH_CASE(ir::Load const& load, SelectionNode& node) {
        resolver(&load);
        return false;
    }
};

template <>
struct Matcher<ir::Store>: MatcherBase {};

template <>
struct Matcher<ir::ConversionInst>: MatcherBase {};

template <>
struct Matcher<ir::CompareInst>: MatcherBase {};

template <>
struct Matcher<ir::UnaryArithmeticInst>: MatcherBase {};

template <>
struct Matcher<ir::ArithmeticInst>: MatcherBase {};

template <>
struct Matcher<ir::Goto>: MatcherBase {};

template <>
struct Matcher<ir::Branch>: MatcherBase {};

template <>
struct Matcher<ir::Return>: MatcherBase {};

template <>
struct Matcher<ir::Call>: MatcherBase {};

template <>
struct Matcher<ir::Phi>: MatcherBase {};

template <>
struct Matcher<ir::Select>: MatcherBase {};

template <>
struct Matcher<ir::GetElementPointer>: MatcherBase {};

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
    std::get<Matcher<ir::Inst>>(matchers).init(resolver);
#include "IR/Lists.def"
    }

    void run();

    /// Emits the instruction \p inst to the current node. Takes ownership of
    /// the pointer
    void emit(mir::Instruction* inst) { instructions.push_back(inst); }

    /// Sets the computed value and selected instruction to the node
    void finalizeNode(SelectionNode& node);

    void match(SelectionNode& node);

    template <typename Inst>
    bool matchImpl(Inst const& inst, SelectionNode& node) {
        return std::get<Matcher<Inst>>(matchers).match(inst, node);
    }
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
    bool matched =
        visit(inst, [&](auto& inst) { return matchImpl(inst, node); });
    if (matched) {
        finalizeNode(node);
    }
    else {
        reportUnmatched(inst);
    }
}

void ISelBlockCtx::finalizeNode(SelectionNode& node) {
    node.setMIR(resolver(node.irInst()), std::move(instructions));
}

#if 0

void BBContext::matchImpl(ir::Call const& call, SelectionNode& node) {
    utl::small_vector<mir::Value*, 16> args;
    for (auto* arg: call.arguments()) {
        auto* mirArg = resolve(*arg);
        size_t const numWords = ::numWords(arg);
        for (size_t i = 0; i < numWords; ++i, mirArg = mirArg->next()) {
            args.push_back(mirArg);
        }
    }
    size_t const numDests = numWords(call.type());
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
    finalizeNode(node);
}

void BBContext::matchImpl(ir::Load const& load, SelectionNode& node) {
    /// This is difficult because we also need to handle oversized types
    utl::small_vector<Matcher<ir::Load>> matchers;
    // Load -> GEP
    matchers.push_back(
        [this](ir::Load const& load, SelectionNode& node) {
        auto* gep = dyncast<ir::GetElementPointer const*>(load.address());
        if (!gep) {
            return false;
        }
        auto& gepNode = *DAG[gep];
        merge(node, gepNode);
        auto source = computeGep(gep);
        size_t size = inst->type()->size();
        emit(new mir::LoadInst(resolve(inst),
                               source,
                               size,
                               inst.metadata()));
        return true;
    });
    // Load (base case)
    matchers.push_back(
        [this](ir::Load const& load, SelectionNode& node) {
        auto* addr = resolve(*inst.rhs());
        size_t size = inst.lhs()->type()->size();
        emit(new mir::ValueArithmeticInst(resolve(inst),
                                          lhs,
                                          rhs,
                                          size,
                                          inst.operation(),
                                          inst.metadata()));
        return true;
    });

    applyMatchers<ir::Load>(matchers, inst, node);
}

void BBContext::matchImpl(ir::ArithmeticInst const& inst, SelectionNode& node) {
    utl::small_vector<Matcher<ir::ArithmeticInst>> matchers;
    // Arithmetic -> Load  [ -> GEP ]
    matchers.push_back(
        [this](ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load) {
            return false;
        }
        auto& loadNode = *DAG[load];
        merge(node, loadNode);
        auto src = [&] {
            auto* gep = dyncast<ir::GetElementPointer const*>(load->address());
            if (gep) {
                auto& gepNode = *DAG[gep];
                merge(node, gepNode);
                return computeGep(gep);
            }
            return mir::MemoryAddress(
                resolveToRegister(load->metadata(), *load->address()));
        }();
        auto* lhs = resolveToRegister(inst.metadata(), *inst.lhs());
        size_t size = inst.lhs()->type()->size();
        emit(new mir::LoadArithmeticInst(resolve(inst),
                                         lhs,
                                         src,
                                         size,
                                         inst.operation(),
                                         inst.metadata()));
        return true;
    });
    // Arithmetic (base case)
    matchers.push_back(
        [this](ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* lhs = resolveToRegister(inst.metadata(), *inst.lhs());
        auto* rhs = resolve(*inst.rhs());
        size_t size = inst.lhs()->type()->size();
        emit(new mir::ValueArithmeticInst(resolve(inst),
                                          lhs,
                                          rhs,
                                          size,
                                          inst.operation(),
                                          inst.metadata()));
        return true;
    });

    applyMatchers<ir::ArithmeticInst>(matchers, inst, node);
}

#endif
