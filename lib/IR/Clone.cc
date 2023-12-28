#include "IR/Clone.h"

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

static Alloca* doClone(Context& context, Alloca* allc) {
    return new Alloca(context,
                      allc->allocatedType(),
                      std::string(allc->name()));
}

static Load* doClone(Context& context, Load* load) {
    return new Load(load->address(), load->type(), std::string(load->name()));
}

static Store* doClone(Context& context, Store* store) {
    return new Store(context, store->address(), store->value());
}

static ConversionInst* doClone(Context& context, ConversionInst* inst) {
    return new ConversionInst(inst->operand(),
                              inst->type(),
                              inst->conversion(),
                              std::string(inst->name()));
}

static CompareInst* doClone(Context& context, CompareInst* cmp) {
    return new CompareInst(context,
                           cmp->lhs(),
                           cmp->rhs(),
                           cmp->mode(),
                           cmp->operation(),
                           std::string(cmp->name()));
}

static UnaryArithmeticInst* doClone(Context& context,
                                    UnaryArithmeticInst* inst) {
    return new UnaryArithmeticInst(context,
                                   inst->operand(),
                                   inst->operation(),
                                   std::string(inst->name()));
}

static ArithmeticInst* doClone(Context& context, ArithmeticInst* inst) {
    return new ArithmeticInst(inst->lhs(),
                              inst->rhs(),
                              inst->operation(),
                              std::string(inst->name()));
}

static Goto* doClone(Context& context, Goto* inst) {
    return new Goto(context, inst->target());
}

static Branch* doClone(Context& context, Branch* inst) {
    return new Branch(context,
                      inst->condition(),
                      inst->thenTarget(),
                      inst->elseTarget());
}

static Return* doClone(Context& context, Return* inst) {
    return new Return(context, inst->value());
}

static Call* doClone(Context& context, Call* inst) {
    return new Call(inst->type(),
                    inst->function(),
                    inst->arguments(),
                    std::string(inst->name()));
}

static Phi* doClone(Context& context, Phi* inst) {
    auto args = inst->arguments() | ToSmallVector<>;
    return new Phi(args, std::string(inst->name()));
}

static GetElementPointer* doClone(Context& context, GetElementPointer* inst) {
    return new GetElementPointer(context,
                                 inst->inboundsType(),
                                 inst->basePointer(),
                                 inst->arrayIndex(),
                                 inst->memberIndices() |
                                     ranges::to<utl::small_vector<size_t>>,
                                 std::string(inst->name()));
}

static Select* doClone(Context& context, Select* inst) {
    return new Select(inst->condition(),
                      inst->thenValue(),
                      inst->elseValue(),
                      std::string(inst->name()));
}

static ExtractValue* doClone(Context& context, ExtractValue* inst) {
    return new ExtractValue(inst->baseValue(),
                            inst->memberIndices() |
                                ranges::to<utl::small_vector<size_t>>,
                            std::string(inst->name()));
}

static InsertValue* doClone(Context& context, InsertValue* inst) {
    return new InsertValue(inst->baseValue(),
                           inst->insertedValue(),
                           inst->memberIndices() |
                               ranges::to<utl::small_vector<size_t>>,
                           std::string(inst->name()));
}

static Instruction* cloneRaw(Context& context, Instruction* inst) {
    return visit(*inst, [&](auto& inst) -> Instruction* {
        return doClone(context, &inst);
    });
}

static BasicBlock* cloneRaw(Context& context,
                            BasicBlock* bb,
                            CloneValueMap& valueMap) {
    auto* result = new BasicBlock(context, std::string(bb->name()));
    for (auto& inst: *bb) {
        auto* cloned = cloneRaw(context, &inst);
        valueMap.add(&inst, cloned);
        result->pushBack(cloned);
    }
    /// While this may look sketchy, we will remap these to their counterparts
    /// in the cloned function later. We just add them here to have something to
    /// remap.
    result->setPredecessors(bb->predecessors());
    return result;
}

UniquePtr<Instruction> ir::clone(Context& context, Instruction* inst) {
    return UniquePtr<Instruction>(cloneRaw(context, inst));
}

UniquePtr<BasicBlock> ir::clone(Context& context, BasicBlock* BB) {
    CloneValueMap valueMap;
    return clone(context, BB, valueMap);
}

UniquePtr<BasicBlock> ir::clone(Context& context,
                                BasicBlock* BB,
                                CloneValueMap& valueMap) {
    auto* result = cloneRaw(context, BB, valueMap);
    for (auto& inst: *result) {
        for (auto* operand: inst.operands()) {
            auto* newVal = valueMap(operand);
            if (newVal != operand) {
                inst.updateOperand(operand, newVal);
            }
        }
    }
    return UniquePtr<BasicBlock>(result);
}

CloneRegionResult ir::cloneRegion(Context& context,
                                  BasicBlock const* insertPoint,
                                  std::span<BasicBlock* const> region) {
    CloneRegionResult result;
    /// Clone the BBs
    for (auto* BB: region) {
        auto* BB2 = clone(context, BB, result.map).release();
        result.map.add(BB, BB2);
        BB->parent()->insert(insertPoint, BB2);
        result.clones.push_back(BB2);
    }
    /// Update all edges in the cloned region
    for (auto* clone: result.clones) {
        for (auto& inst: *clone) {
            for (auto [index, op]: inst.operands() | ranges::views::enumerate) {
                inst.setOperand(index, result.map(op));
            }
            if (auto* phi = dyncast<Phi*>(&inst)) {
                phi->mapPredecessors(result.map);
            }
        }
        clone->mapPredecessors(result.map);
    }
    return result;
}

UniquePtr<Function> ir::clone(Context& context, Function* function) {
    // TODO: Use actual type here when we have proper function types
    // auto* type = cast<FunctionType const*>(function->type());
    auto paramTypes =
        function->parameters() |
        ranges::views::transform([](Parameter const& p) { return p.type(); }) |
        ToSmallVector<>;
    auto result = allocate<Function>(context,
                                     function->returnType(),
                                     paramTypes,
                                     std::string(function->name()),
                                     function->attributes());
    CloneValueMap valueMap;
    for (auto& bb: *function) {
        auto* cloned = ::cloneRaw(context, &bb, valueMap);
        valueMap.add(&bb, cloned);
        result->pushBack(cloned);
    }
    for (auto&& [oldParam, newParam]:
         ranges::views::zip(function->parameters(), result->parameters()))
    {
        valueMap.add(&oldParam, &newParam);
    }
    for (auto& bb: *result) {
        auto newPreds = bb.predecessors() | ranges::views::transform(valueMap) |
                        ToSmallVector<>;
        bb.setPredecessors(newPreds);
        for (auto& inst: bb) {
            for (auto&& [index, operand]:
                 inst.operands() | ranges::views::enumerate) {
                inst.setOperand(index, valueMap(operand));
            }
            /// It is quite unfortunate that we have this special case here, but
            /// `phi` is the only instruction that has links invisible to
            /// `Instruction` base class.
            if (auto* phi = dyncast<Phi*>(&inst)) {
                for (auto&& [index, arg]:
                     phi->arguments() | ranges::views::enumerate) {
                    phi->setPredecessor(index, valueMap(arg.pred));
                }
            }
        }
    }
    return result;
}
