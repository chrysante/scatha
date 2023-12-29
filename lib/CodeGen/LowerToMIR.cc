#include "CodeGen/Passes.h"

#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISel.h"
#include "CodeGen/ISelCommon.h"
#include "CodeGen/Resolver.h"
#include "CodeGen/SelectionDAG.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;
using namespace ranges::views;

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
    LoweringOptions options;

    ValueMap valueMap;

    LoweringContext(ir::Module const& irMod,
                    mir::Context& ctx,
                    mir::Module& mirMod,
                    LoweringOptions options):
        irMod(irMod), ctx(ctx), mirMod(mirMod), options(options) {}

    void run();

    mir::Function* declareFunction(ir::Function const& irFn);

    mir::BasicBlock* declareBB(mir::Function& mirFn,
                               ir::BasicBlock const& irBB);

    void generateAllocas(ir::Function const& irFn, mir::Function& mirFn);

    void generateBB(ir::BasicBlock const& irBB);

    /// Perform instruction scheduling of the selection dag \p DAG
    /// Right now this is simply a linearization in a topsort order
    void schedule(SelectionDAG& DAG, mir::BasicBlock& mirBB);
};

} // namespace

mir::Module cg::lowerToMIR(mir::Context& ctx,
                           ir::Module const& irMod,
                           LoweringOptions options) {
    mir::Module mirMod;
    LoweringContext(irMod, ctx, mirMod, options).run();
    return mirMod;
}

void LoweringContext::run() {
    /// Make forward declarations of all functions and basic blocks
    for (auto& irFn: irMod) {
        auto* mirFn = declareFunction(irFn);
        for (auto& irBB: irFn) {
            declareBB(*mirFn, irBB);
        }
        generateAllocas(irFn, *mirFn);
    }
    /// Perform instruction selection and scheduling for each basic block
    for (auto& irFn: irMod) {
        for (auto& irBB: irFn) {
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
    valueMap.addValue(&irFn, mirFn);
    /// Associate parameters with bottom registers
    auto regItr = mirFn->ssaRegisters().begin();
    for (auto& param: irFn.parameters()) {
        valueMap.addValue(&param, regItr.to_address());
        std::advance(regItr, numWords(param));
    }
    return mirFn;
}

mir::BasicBlock* LoweringContext::declareBB(mir::Function& mirFn,
                                            ir::BasicBlock const& irBB) {
    auto* mirBB = new mir::BasicBlock(&irBB);
    mirFn.pushBack(mirBB);
    valueMap.addValue(&irBB, mirBB);
    return mirBB;
}

static size_t alignTo(size_t size, size_t align) {
    if (size % align == 0) {
        return size;
    }
    return size + align - size % align;
}

void LoweringContext::generateAllocas(ir::Function const& irFn,
                                      mir::Function& mirFn) {
    utl::small_vector<ir::Alloca const*> allocas;
    for (auto& inst: irFn.entry()) {
        auto* allocaInst = dyncast<ir::Alloca const*>(&inst);
        if (!allocaInst) {
            break;
        }
        allocas.push_back(allocaInst);
    }
    if (allocas.empty()) {
        return;
    }
    SC_ASSERT(ranges::all_of(allocas, &ir::Alloca::isStatic),
              "For now we only support lowering static allocas");
    /// Compute the offsets for the allocas
    utl::small_vector<size_t> offsets;
    size_t numBytes = 0;
    for (auto* inst: allocas) {
        offsets.push_back(numBytes);
        auto size = inst->allocatedSize();
        size_t const StaticAllocaAlign = 16;
        numBytes += alignTo(*size, StaticAllocaAlign);
    }

    /// Emit one LISP instruction
    Resolver resolver(ctx, mirMod, mirFn, valueMap, [&](mir::Instruction*) {
        SC_UNREACHABLE();
    });
    mir::Register* baseptr = resolver.nextRegister();
    auto* lispInst = new mir::LISPInst(baseptr, ctx.constant(numBytes, 2), {});
    mirFn.entry()->pushBack(lispInst);

    /// Store the results
    for (auto [allocaInst, offset]: zip(allocas, offsets)) {
        valueMap.addAddress(allocaInst, baseptr, offset);
    }
}

void LoweringContext::generateBB(ir::BasicBlock const& irBB) {
    auto& mirBB = cast<mir::BasicBlock&>(*valueMap.getValue(&irBB));
    for (auto* pred: irBB.predecessors()) {
        mirBB.addPredecessor(cast<mir::BasicBlock*>(valueMap.getValue(pred)));
    }
    for (auto* succ: irBB.successors()) {
        mirBB.addSuccessor(cast<mir::BasicBlock*>(valueMap.getValue(succ)));
    }
    auto DAG = SelectionDAG::Build(irBB);
    if (options.generateSelectionDAGImages) {
        generateGraphvizTmp(DAG, std::string(irBB.name()));
    }
    isel(DAG, ctx, mirMod, *mirBB.parent(), valueMap);
    if (options.generateSelectionDAGImages) {
        generateGraphvizTmp(DAG, utl::strcat(irBB.name(), "-selected"));
    }
    schedule(DAG, mirBB);
}

void LoweringContext::schedule(SelectionDAG& DAG, mir::BasicBlock& BB) {
    auto nodes = DAG.topsort();
    for (auto* node: nodes | reverse) {
        auto instructions = node->extractInstructions();
        auto insertPoint = [&] {
            if (isa<ir::Phi>(node->irInst())) {
                return BB.phiNodes().end();
            }
            return BB.end();
        }();
        BB.splice(insertPoint, instructions.begin(), instructions.end());
    }
}
