#include "CodeGen/Passes.h"

#include <range/v3/numeric.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISel.h"
#include "CodeGen/ISelCommon.h"
#include "CodeGen/SelectionDAG.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

static size_t numParamRegisters(ir::Function const& F) {
    return ranges::accumulate(F.parameters(),
                              size_t(0),
                              ranges::plus{},
                              [&](auto& param) { return numWords(param); });
}

static size_t numReturnRegisters(ir::Function const& F) {
    return numWords(*F.returnType());
}

namespace {

struct LoweringContext {
    ir::Module const& irMod;
    mir::Context& ctx;
    mir::Module& mirMod;

    ValueMap map;

    LoweringContext(ir::Module const& irMod,
                    mir::Context& ctx,
                    mir::Module& mirMod):
        irMod(irMod), ctx(ctx), mirMod(mirMod) {}

    void run();

    mir::Function* declareFunction(ir::Function const& irFn);

    mir::BasicBlock* declareBB(mir::Function& mirFn,
                               ir::BasicBlock const& irBB);

    void generateBB(ir::BasicBlock const& irBB);

    /// Perform instruction scheduling of the selection dag \p DAG
    /// Right now this is simply a linearization in a topsort order
    void schedule(SelectionDAG& DAG, mir::BasicBlock& mirBB);
};

} // namespace

mir::Module cg::lowerToMIR2(mir::Context& ctx, ir::Module const& irMod) {
    mir::Module mirMod;
    LoweringContext(irMod, ctx, mirMod).run();
    return mirMod;
}

void LoweringContext::run() {
    utl::small_vector<std::pair<ir::Function const*, mir::Function*>> functions;
    for (auto& irFn: irMod) {
        auto* mirFn = declareFunction(irFn);
        functions.push_back({ &irFn, mirFn });
    }
    for (auto [irFn, mirFn]: functions) {
        for (auto& irBB: *irFn) {
            declareBB(*mirFn, irBB);
        }
        for (auto& irBB: *irFn) {
            generateBB(irBB);
        }
    }
}

mir::Function* LoweringContext::declareFunction(ir::Function const& irFn) {
    auto* mirFn = new mir::Function(&irFn,
                                    numParamRegisters(irFn),
                                    numReturnRegisters(irFn),
                                    irFn.visibility());
    mirMod.addFunction(mirFn);
    map.insert(&irFn, mirFn);
    /// Associate parameters with bottom registers
    auto regItr = mirFn->ssaRegisters().begin();
    for (auto& param: irFn.parameters()) {
        map.insert(&param, regItr.to_address());
        std::advance(regItr, numWords(param));
    }
    return mirFn;
}

mir::BasicBlock* LoweringContext::declareBB(mir::Function& mirFn,
                                            ir::BasicBlock const& irBB) {
    auto* mirBB = new mir::BasicBlock(&irBB);
    mirFn.pushBack(mirBB);
    map.insert(&irBB, mirBB);
    return mirBB;
}

void LoweringContext::generateBB(ir::BasicBlock const& irBB) {
    auto& mirBB = cast<mir::BasicBlock&>(*map(&irBB));
    for (auto* pred: irBB.predecessors()) {
        mirBB.addPredecessor(cast<mir::BasicBlock*>(map(pred)));
    }
    for (auto* succ: irBB.successors()) {
        mirBB.addSuccessor(cast<mir::BasicBlock*>(map(succ)));
    }
    auto DAG = SelectionDAG::Build(irBB);
    isel(DAG, ctx, mirMod, *mirBB.parent(), map);
    schedule(DAG, mirBB);
}

void LoweringContext::schedule(SelectionDAG& DAG, mir::BasicBlock& BB) {
    auto nodes = DAG.topsort();
    for (auto* node: nodes | ranges::views::reverse) {
        auto instructions = node->extractInstructions();
        auto insertPoint = [&] {
            if (isa<ir::Alloca>(node->irInst())) {
                return BB.begin();
            }
            if (isa<ir::Phi>(node->irInst())) {
                return BB.phiNodes().end();
            }
            return BB.end();
        }();
        BB.splice(insertPoint, instructions.begin(), instructions.end());
    }
}
