#include "Opt/ConstantPropagation.h"

#include <deque>
#include <numeric>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/variant.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

/// Implemented with help from:
/// https://karkare.github.io/cs738/lecturenotes/11CondConstPropHandout.pdf
/// https://www.cs.utexas.edu/users/lin/cs380c/wegman.pdf

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct FlowEdge {
    BasicBlock* origin;
    BasicBlock* dest;

    bool operator==(FlowEdge const&) const = default;
};

struct UseEdge {
    Value* value;
    User* user;
};

} // namespace

template <>
struct std::hash<FlowEdge> {
    std::size_t operator()(FlowEdge const& e) const { return utl::hash_combine(e.origin, e.dest); }
};

namespace {

enum class Inevaluable {};

/// Supremum, evaluation
enum class Unexamined {};

using FormalValue = utl::variant<Unexamined, Inevaluable, APInt, APFloat>;

bool isUnexamined(FormalValue const& value) {
    return value.index() == 0;
}

[[maybe_unused]] bool isInevaluable(FormalValue const& value) {
    return value.index() == 1;
}

bool isConstant(FormalValue const& value) {
    return value.index() == 2 || value.index() == 3;
}

FormalValue infimum(FormalValue const& a, FormalValue const& b) {
    if (isUnexamined(a)) {
        return b;
    }
    if (isUnexamined(b)) {
        return a;
    }
    if (a == b) {
        return a;
    }
    return Inevaluable{};
}

FormalValue infimum(utl::range_for<FormalValue> auto&& range) {
    return std::accumulate(++std::begin(range), std::end(range), *std::begin(range), [](auto const& a, auto const& b) {
        return infimum(a, b);
    });
}

/// One context object is created per analyzed function.
struct SCCContext {
    explicit SCCContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}

    void run();

    void processFlowEdge(FlowEdge edge);

    void processUseEdge(UseEdge edge);

    void visitPhi(Phi& phi);

    void visitExpressions(BasicBlock& basicBlock);

    void visitExpression(Instruction& inst);

    void notifyUsers(Value& value);

    void processTerminator(FormalValue const& value, TerminatorInst& inst);

    void addSingleEdge(APInt const& constant, TerminatorInst& inst);

    bool basicBlockIsExecutable(BasicBlock& basicBlock);

    size_t numIncomingExecutableEdges(BasicBlock& basicBlock);

    FormalValue evaluateArithmetic(ArithmeticOperation operation, FormalValue const& lhs, FormalValue const& rhs);

    FormalValue evaluateUnaryArithmetic(UnaryArithmeticOperation operation, FormalValue const& operand);

    FormalValue evaluateComparison(CompareOperation operation, FormalValue const& lhs, FormalValue const& rhs);

    bool isExpression(Instruction const* inst) const {
        return inst != nullptr && !isa<Phi>(inst) && !isa<TerminatorInst>(inst);
    }

    bool isExecutable(FlowEdge const& e) { return execMap.insert({ e, false }).first->second; }

    void setExecutable(FlowEdge const& e, bool value) { execMap.insert_or_assign(e, value); }

    FormalValue formalValue(Value* value);

    void setFormalValue(Value* value, FormalValue formalValue) { formalValues.insert_or_assign(value, formalValue); }

    Context& irCtx;
    Function& function;

    std::deque<FlowEdge> flowWorklist;
    std::deque<UseEdge> useWorklist;
    utl::hashmap<Value*, FormalValue> formalValues;
    utl::hashmap<FlowEdge, bool> execMap;
};

} // namespace

void opt::propagateConstants(ir::Context& context, ir::Function& function) {
    SCCContext ctx(context, function);
    ctx.run();
    assertInvariants(context, function);
}

void SCCContext::run() {
    auto& entry = function.entry();
    flowWorklist.push_back({ nullptr, &entry });
    while (!flowWorklist.empty() || !useWorklist.empty()) {
        if (!flowWorklist.empty()) {
            FlowEdge const edge = flowWorklist.front();
            flowWorklist.pop_front();
            processFlowEdge(edge);
        }
        else if (!useWorklist.empty()) {
            UseEdge const edge = useWorklist.front();
            useWorklist.pop_front();
            processUseEdge(edge);
        }
    }
    utl::small_vector<Instruction*> replacedInstructions;
    for (auto [value, latticeElement]: formalValues) {
        if (!isConstant(latticeElement)) {
            continue;
        }
        if (isa<Constant>(value)) {
            continue;
        }
        SC_ASSERT(isa<Instruction>(value), "We can only replace instructions");
        size_t const bitWidth = cast<ArithmeticType const*>(value->type())->bitWidth();
        // clang-format off
        Value* const newValue = visit(utl::overload{
            [&](APInt const& constant) {
                return irCtx.integralConstant(constant, bitWidth);
            },
            [&](APFloat const& constant) {
                return irCtx.floatConstant(constant, bitWidth);
            },
            [](auto const&) -> Value* { SC_UNREACHABLE(); }
        }, latticeElement); // clang-format on
        replaceValue(value, newValue);
        replacedInstructions.push_back(cast<Instruction*>(value));
    }
    for (auto* inst: replacedInstructions) {
        if (!inst->users().empty()) {
            continue;
        }
        inst->parent()->instructions.erase(inst);
        inst->clearOperands();
    }
}

void SCCContext::processFlowEdge(FlowEdge edge) {
    if (isExecutable(edge)) {
        return;
    }
    setExecutable(edge, true);
    auto const [origin, dest] = edge;
    for (auto& phi: dest->phis()) {
        visitPhi(phi);
    }
    if (dest->isEntry() || numIncomingExecutableEdges(*dest) == 1) {
        visitExpressions(*dest);
    }
    if (dest->successors().size() == 1) {
        flowWorklist.push_back({ dest, dest->successors().front() });
    }
}

void SCCContext::processUseEdge(UseEdge edge) {
    // clang-format off
    visit(*edge.user, utl::overload{
        [&](Phi& phi) { visitPhi(phi); },
        [&](Instruction& inst) {
            if (basicBlockIsExecutable(*inst.parent())) {
                visitExpression(inst);
            }
        },
        [](TerminatorInst const&) {},
        [](Value const&) { SC_UNREACHABLE(); }
    }); // clang-format on
}

void SCCContext::visitPhi(Phi& phi) {
    FormalValue const value =
        infimum(utl::transform(phi.arguments(), [this, bb = phi.parent()](PhiMapping arg) -> FormalValue {
            if (isExecutable({ arg.pred, bb })) {
                return formalValue(arg.value);
            }
            return Unexamined{};
        }));
    if (value == formalValue(&phi)) {
        return;
    }
    setFormalValue(&phi, value);
    notifyUsers(phi);
}

void SCCContext::visitExpressions(BasicBlock& basicBlock) {
    for (auto& inst: basicBlock.instructions) {
        if (!isExpression(&inst)) {
            continue;
        }
        visitExpression(inst);
    }
}

void SCCContext::visitExpression(Instruction& inst) {
    SC_ASSERT(isExpression(&inst), "");
    FormalValue const oldValue = formalValue(&inst);
    // clang-format off
    FormalValue const value = visit(inst, utl::overload{
        [&](ArithmeticInst& inst) {
            return evaluateArithmetic(inst.operation(), formalValue(inst.lhs()), formalValue(inst.rhs()));
        },
        [&](UnaryArithmeticInst& inst) {
            return evaluateUnaryArithmetic(inst.operation(), formalValue(inst.operand()));
        },
        [&](CompareInst& inst) {
            return evaluateComparison(inst.operation(), formalValue(inst.lhs()), formalValue(inst.rhs()));
        },
        [&](Instruction const&) -> FormalValue { return Inevaluable{}; }
    }); // clang-format on
    if (value == oldValue) {
        return;
    }
    setFormalValue(&inst, value);
    notifyUsers(inst);
}

void SCCContext::notifyUsers(Value& value) {
    for (auto* user: value.users()) {
        if (auto* phi = dyncast<Phi*>(user)) {
            visitPhi(*phi);
        }
        else if (isExpression(dyncast<Instruction const*>(user))) {
            useWorklist.push_back({ &value, user });
        }
        else if (auto* term = dyncast<TerminatorInst*>(user)) {
            processTerminator(formalValue(&value), *term);
        }
    }
}

void SCCContext::processTerminator(FormalValue const& value, TerminatorInst& inst) {
    // clang-format off
    utl::visit(utl::overload{
        [&](Unexamined) {
            SC_UNREACHABLE();
        },
        [&](APInt const& constant) {
            addSingleEdge(constant, inst);
        },
        [&](APFloat const& constant) {
            SC_ASSERT(isa<Return>(inst), "Float can atmost control return instructions");
        },
        [&](Inevaluable) {
            std::transform(inst.targets().begin(),
                           inst.targets().end(),
                           std::back_inserter(flowWorklist),
                           [&](BasicBlock* target) {
                FlowEdge const edge = { inst.parent(), target };
                setExecutable(edge, false);
                return edge;
            });
        },
    }, value); // clang-format on
}

void SCCContext::addSingleEdge(APInt const& constant, TerminatorInst& inst) {
    // clang-format off
    visit(inst, utl::overload{
        [&](Goto& gt)   {
            flowWorklist.push_back({ inst.parent(), gt.target() });
        },
        [&](Branch& br) {
            SC_ASSERT(constant == 0 || constant == 1, "Boolean constant must be 0 or 1");
            BasicBlock* const origin = br.parent();
            size_t const index = 1 - constant.to<size_t>();
            BasicBlock* const target = br.targets()[index];
            FlowEdge const edge = { origin, target };
            setExecutable(edge, false);
            flowWorklist.push_back(edge);
        },
        [&](Return& inst) {},
        [](TerminatorInst const&) { SC_UNREACHABLE(); }
    }); // clang-format on
}

bool SCCContext::basicBlockIsExecutable(BasicBlock& bb) {
    if (bb.isEntry()) {
        return true;
    }
    for (auto* pred: bb.predecessors) {
        if (isExecutable({ pred, &bb })) {
            return true;
        }
    }
    return false;
}

size_t SCCContext::numIncomingExecutableEdges(BasicBlock& basicBlock) {
    size_t result = 0;
    for (auto* pred: basicBlock.predecessors) {
        result += isExecutable({ pred, &basicBlock });
    }
    return result;
}

template <typename T>
concept FormalConstant = std::same_as<T, APInt> || std::same_as<T, APFloat>;

FormalValue SCCContext::evaluateArithmetic(ArithmeticOperation operation,
                                           FormalValue const& lhs,
                                           FormalValue const& rhs) {
    switch (operation) {
    case ArithmeticOperation::Add: [[fallthrough]];
    case ArithmeticOperation::Sub:
        // clang-format off
        return utl::visit(utl::overload{
            [&]<FormalConstant T>(T const& lhs, T const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Add: return lhs + rhs;
                case ArithmeticOperation::Sub: return lhs - rhs;
                default: SC_UNREACHABLE();
                }
            },
            [](Inevaluable, auto const&) -> FormalValue { return Inevaluable{}; },
            [](auto const&, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Unexamined{}; },
        }, lhs, rhs); // clang-format on
    case ArithmeticOperation::Mul: [[fallthrough]];
    case ArithmeticOperation::Div: [[fallthrough]];
    case ArithmeticOperation::Rem:
        // clang-format off
        return utl::visit(utl::overload{
            [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Mul: return lhs * rhs;
                case ArithmeticOperation::Div: return lhs / rhs;
                case ArithmeticOperation::Rem: return lhs % rhs;
                default: SC_UNREACHABLE();
                }
            },
            [&](APFloat const& lhs, APFloat const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Mul: return lhs * rhs;
                case ArithmeticOperation::Div: return lhs / rhs;
                case ArithmeticOperation::Rem: SC_UNREACHABLE();
                default: SC_UNREACHABLE();
                }
            },
            []<typename T>(APInt const& lhs, T const& rhs) -> FormalValue {
                if (lhs == 0) {
                    return APInt(0);
                }
                return T{};
            },
            []<typename T>(T const& lhs, APInt const& rhs) -> FormalValue {
                /// TODO: Here we should return 'undef' for division and remainder.
                if (rhs == 0) {
                    return APInt(0);
                }
                return T{};
            },
            [](Inevaluable, Unexamined)  -> FormalValue { return Inevaluable{}; },
            [](Unexamined,  Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
        }, lhs, rhs); // clang-format on

    default: return Inevaluable{};
    }
}

FormalValue SCCContext::evaluateUnaryArithmetic(UnaryArithmeticOperation operation, FormalValue const& operand) {
    // clang-format off
    return utl::visit(utl::overload{
        [&](APInt const& operand) -> FormalValue {
            switch (operation) {
            case UnaryArithmeticOperation::Promotion: return +operand;
            case UnaryArithmeticOperation::Negation: return -operand;
            case UnaryArithmeticOperation::BitwiseNot: return ~operand;
            case UnaryArithmeticOperation::LogicalNot:
                SC_ASSERT(operand == 0 || operand == 1, "Operand must be boolean");
                return 1 - operand;
            case UnaryArithmeticOperation::_count: SC_UNREACHABLE();
            }
        },
        [&](APFloat const& operand) -> FormalValue {
            switch (operation) {
            case UnaryArithmeticOperation::Promotion: return +operand;
            case UnaryArithmeticOperation::Negation: return -operand;
            case UnaryArithmeticOperation::BitwiseNot: [[fallthrough]];
            case UnaryArithmeticOperation::LogicalNot: [[fallthrough]];
            case UnaryArithmeticOperation::_count: SC_UNREACHABLE();
            }
        },
        [](auto const&) -> FormalValue { return Inevaluable{}; },
    }, operand); // clang-format on
}

FormalValue SCCContext::evaluateComparison(CompareOperation operation, FormalValue const& lhs, FormalValue const& rhs) {
    // clang-format off
    return utl::visit(utl::overload{
        [&]<FormalConstant T>(T const& lhs, T const& rhs) -> FormalValue {
            switch (operation) {
            case CompareOperation::Less:      return APInt(lhs <  rhs);
            case CompareOperation::LessEq:    return APInt(lhs <= rhs);
            case CompareOperation::Greater:   return APInt(lhs >  rhs);
            case CompareOperation::GreaterEq: return APInt(lhs >= rhs);
            case CompareOperation::Equal:     return APInt(lhs == rhs);
            case CompareOperation::NotEqual:  return APInt(lhs != rhs);
            case CompareOperation::_count: SC_UNREACHABLE();
            }
        },
        [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
    }, lhs, rhs); // clang-format on
}

FormalValue SCCContext::formalValue(Value* value) {
    auto const [itr, justAdded] = formalValues.insert({ value, Unexamined{} });
    auto&& [key, formalValue]   = *itr;
    if (!justAdded) {
        return formalValue;
    }
    // clang-format off
    formalValue = visit(*value, utl::overload{
        [&](IntegralConstant const& constant) -> FormalValue {
            return constant.value();
        },
        [&](FloatingPointConstant const& constant) -> FormalValue {
            return constant.value();
        },
        [&](Parameter const& param) -> FormalValue { return Inevaluable{}; },
        [&](Value const& other) -> FormalValue { return Unexamined{}; }
    }); // clang-format on
    return formalValue;
}
