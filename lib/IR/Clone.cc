#include "IR/Clone.h"

#include <utl/hashtable.hpp>

#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

static Instruction* doClone(Context&, Instruction*) { SC_UNREACHABLE(); }

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

static CompareInst* doClone(Context& context, CompareInst* cmp) {
    return new CompareInst(context,
                           cmp->lhs(),
                           cmp->rhs(),
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

static FunctionCall* doClone(Context& context, FunctionCall* inst) {
    return new FunctionCall(inst->function(),
                            inst->arguments(),
                            std::string(inst->name()));
}

static ExtFunctionCall* doClone(Context& context, ExtFunctionCall* inst) {
    return new ExtFunctionCall(inst->slot(),
                               inst->index(),
                               std::string(inst->functionName()),
                               inst->arguments(),
                               inst->type(),
                               std::string(inst->name()));
}

static Phi* doClone(Context& context, Phi* inst) {
    auto args = inst->arguments() | ranges::to<utl::small_vector<PhiMapping>>;
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
    SC_DEBUGFAIL();
}

static InsertValue* doClone(Context& context, InsertValue* inst) {
    SC_DEBUGFAIL();
}

static Instruction* clone(Context& context, Instruction* inst) {
    return visit(*inst, [&](auto& inst) { return doClone(context, &inst); });
}

using ValueMap = utl::hashmap<Value*, Value*>;

static BasicBlock* clone(Context& context, BasicBlock* bb, ValueMap& valueMap) {
    auto result = new BasicBlock(context, std::string(bb->name()));
    for (auto& inst: *bb) {
        auto* cloned    = clone(context, &inst);
        valueMap[&inst] = cloned;
        result->pushBack(cloned);
    }
    /// While this may look sketchy, we will remap these to their counterparts
    /// in the cloned function later. We just add them here to have something to
    /// remap.
    result->setPredecessors(bb->predecessors());
    return result;
}

UniquePtr<Function> ir::clone(Context& context, Function* function) {
    // TODO: Use actual type here when we have proper function types
    // auto* type = cast<FunctionType const*>(function->type());
    auto paramTypes =
        function->parameters() |
        ranges::views::transform([](Parameter const& p) { return p.type(); }) |
        ranges::to<utl::small_vector<Type const*>>;
    auto result = allocate<Function>(nullptr,
                                     function->returnType(),
                                     paramTypes,
                                     std::string(function->name()));
    ValueMap valueMap;
    for (auto& bb: *function) {
        auto* cloned  = ::clone(context, &bb, valueMap);
        valueMap[&bb] = cloned;
        result->pushBack(cloned);
    }
    for (auto&& [oldParam, newParam]:
         ranges::views::zip(function->parameters(), result->parameters()))
    {
        valueMap[&oldParam] = &newParam;
    }
    auto map = [&]<typename T>(T* value) {
        auto itr = valueMap.find(value);
        if (itr == valueMap.end()) {
            return value;
        }
        return cast<T*>(itr->second);
    };
    for (auto& bb: *result) {
        auto newPreds = bb.predecessors() | ranges::views::transform(map) |
                        ranges::to<utl::small_vector<BasicBlock*>>;
        bb.setPredecessors(newPreds);
        for (auto& inst: bb) {
            for (auto&& [index, operand]:
                 inst.operands() | ranges::views::enumerate)
            {
                inst.setOperand(index, map(operand));
            }
            /// It is quite unfortunate that we have this special case here, but
            /// `phi` is the only instruction that has links invisible to
            /// `Instruction` base class.
            if (auto* phi = dyncast<Phi*>(&inst)) {
                for (auto&& [index, arg]:
                     phi->arguments() | ranges::views::enumerate)
                {
                    phi->setPredecessor(index, map(arg.pred));
                }
            }
        }
    }
    return result;
}
