#include "Opt/ConstantPropagation.h"

#include <deque>
#include <numeric>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/variant.hpp>

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

enum class Inevaluable{};

/// Supremum, evaluation
enum class Unexamined{};

using FormalValue = utl::variant<Unexamined, Inevaluable, APInt>;

bool isUnexamined(FormalValue const& value) { return value.index() == 0; }

bool isInevaluable(FormalValue const& value) { return value.index() == 1; }

bool isConstant(FormalValue const& value) { return value.index() == 2; }

FormalValue infimum(FormalValue const& a, FormalValue const& b) {
    if (isUnexamined(a)) { return b; }
    if (isUnexamined(b)) { return a; }
    if (a == b) { return a; }
    return Inevaluable{};
}

FormalValue infimum(utl::range_for<FormalValue> auto&& range) {
    return std::accumulate(++std::begin(range), std::end(range), *std::begin(range), [](auto const& a, auto const& b) { return infimum(a, b); });
}

/// One context object is created per analyzed function.
struct SCCContext {
    explicit SCCContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}

    void run();

    void processFlowEdge(FlowEdge edge);
    
    void visitPhi(Phi& phi);

    void visitExpressions(BasicBlock& basicBlock);
    
    void visitExpression(Instruction& inst);
    
    void processTerminator(FormalValue const& value, TerminatorInst& inst);
    
    void addSingleEdge(APInt const& constant, TerminatorInst& inst);
    
    void processUseEdge(UseEdge edge);

    bool basicBlockIsExecutable(BasicBlock& basicBlock);

    size_t numIncomingExecutableEdges(BasicBlock& basicBlock);
    
    FormalValue evaluateArithmetic(ArithmeticOperation operation, FormalValue const& lhs, FormalValue const& rhs);
    
    FormalValue evaluateComparison(CompareOperation operation, FormalValue const& lhs, FormalValue const& rhs);
    
    bool isExpression(Instruction const* inst) const { return inst != nullptr && !isa<Phi>(inst) && !isa<TerminatorInst>(inst); }
    
    bool isExecutable(FlowEdge const& e) { return execMap.insert({ e, false }).first->second; }

    void setExecutable(FlowEdge const& e, bool value) { execMap.insert_or_assign(e, value); }

    FormalValue formalValue(Value const* value);
    
    void setFormalValue(Value const* value, FormalValue formalValue) { formalValues.insert_or_assign(value, formalValue); }
    
    Context& irCtx;
    Function& function;
    
    std::deque<FlowEdge> flowWorklist;
    std::deque<UseEdge> useWorklist;
    utl::hashmap<Value const*, FormalValue> formalValues;
    utl::hashmap<FlowEdge, bool> execMap;
};

} // namespace

void opt::propagateConstants(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        SCCContext ctx(context, function);
        ctx.run();
    }
    assertInvariants(context, mod);
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
        if (!useWorklist.empty()) {
            UseEdge const edge = useWorklist.front();
            useWorklist.pop_front();
            processUseEdge(edge);
        }
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

void SCCContext::visitPhi(Phi& phi) {
    FormalValue const value = infimum(utl::transform(phi.arguments(), [this, bb = phi.parent()](PhiMapping arg) -> FormalValue {
        if (isExecutable({ arg.pred, bb }))  {
            return formalValue(arg.value);
        }
        return Unexamined{};
    }));
    
    if (isConstant(value)) {
        auto* newValue = irCtx.integralConstant(value.get<APInt>(), cast<IntegralType const*>(phi.type())->bitWidth());
        replaceValue(&phi, newValue);
        phi.clearOperands();
        phi.parent()->instructions.erase(&phi);
        return;
    }
    if (auto const operands = phi.operands();
        std::adjacent_find(operands.begin(), operands.end(), std::not_equal_to<>{}) == operands.end())
    {
        auto* newValue = *operands.begin();
        replaceValue(&phi, newValue);
        phi.clearOperands();
        phi.parent()->instructions.erase(&phi);
        return;
    }
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
            auto result = evaluateArithmetic(inst.operation(), formalValue(inst.lhs()), formalValue(inst.rhs()));
            return result;
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
    for (auto* user: inst.users()) {
        if (isExpression(dyncast<Instruction const*>(user))) {
            useWorklist.push_back({ &inst, user });
        }
        else if (auto* term = dyncast<TerminatorInst*>(user)) {
            processTerminator(value, *term);
        }
    }
    if (isConstant(value)) {
        APInt const& constant = value.get<APInt>();
        auto* newValue = irCtx.integralConstant(constant, cast<IntegralType const*>(inst.type())->bitWidth());
        replaceValue(&inst, newValue);
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
        [&](Inevaluable) {
            std::transform(inst.targets().begin(),
                           inst.targets().end(),
                           std::back_inserter(flowWorklist),
                           [&](BasicBlock* target) { return FlowEdge{ inst.parent(), target }; });
        },
    }, value);// clang-format on
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
            size_t const index = static_cast<size_t>(constant);
            BasicBlock* const target = br.targets()[index];
            flowWorklist.push_back({ origin, target });
        },
        [&](Return& inst) {},
        [](TerminatorInst const&) { SC_UNREACHABLE(); }
    }); // clang-format on
}

bool SCCContext::basicBlockIsExecutable(BasicBlock& bb) {
    if (bb.isEntry()) { return true; }
    for (auto* pred: bb.predecessors) {
        if (isExecutable({ pred, &bb })) {
            return true;
        }
    }
    return false;
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

size_t SCCContext::numIncomingExecutableEdges(BasicBlock& basicBlock) {
    size_t result = 0;
    for (auto* pred: basicBlock.predecessors) {
        result += isExecutable({ pred, &basicBlock });
    }
    return result;
}

FormalValue SCCContext::evaluateArithmetic(ArithmeticOperation operation, FormalValue const& lhs, FormalValue const& rhs) {
    switch (operation) {
    case ArithmeticOperation::Add: [[fallthrough]];
    case ArithmeticOperation::Sub:
        return utl::visit(utl::overload{
            [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
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
        }, lhs, rhs);
    case ArithmeticOperation::Mul: [[fallthrough]];
    case ArithmeticOperation::Div: [[fallthrough]];
    case ArithmeticOperation::Rem:
        return utl::visit(utl::overload{
            [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Mul: return lhs * rhs;
                case ArithmeticOperation::Div: return lhs / rhs;
                case ArithmeticOperation::Rem: return lhs % rhs;
                default: SC_UNREACHABLE();
                }
            },
            []<typename T>(APInt const& lhs, T const& rhs) -> FormalValue {
                if (lhs == 0) {
                    return 0;
                }
                return T{};
            },
            []<typename T>(T const& lhs, APInt const& rhs) -> FormalValue {
                /// TODO: Here we should return 'undef' for division and remainder.
                if (rhs == 0) {
                    return 0;
                }
                return T{};
            },
            [](Inevaluable, Unexamined)  -> FormalValue { return Inevaluable{}; },
            [](Unexamined,  Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
        }, lhs, rhs);
        
    default:
        return Inevaluable{};
    }
}

FormalValue SCCContext::evaluateComparison(CompareOperation operation, FormalValue const& lhs, FormalValue const& rhs) {
    return utl::visit(utl::overload{
        [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
            switch (operation) {
            case CompareOperation::Less:      return lhs <  rhs;
            case CompareOperation::LessEq:    return lhs <= rhs;
            case CompareOperation::Greater:   return lhs >  rhs;
            case CompareOperation::GreaterEq: return lhs >= rhs;
            case CompareOperation::Equal:     return lhs == rhs;
            case CompareOperation::NotEqual:  return lhs != rhs;
            case CompareOperation::_count: SC_UNREACHABLE();
            }
        },
        [](Inevaluable, auto const&) -> FormalValue { return Inevaluable{}; },
        [](auto const&, Inevaluable) -> FormalValue { return Inevaluable{}; },
        [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
        [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
    }, lhs, rhs);
}

FormalValue SCCContext::formalValue(Value const* value) {
    auto const [itr, justAdded] = formalValues.insert({ value, Unexamined{} });
    auto&& [key, formalValue] = *itr;
    if (!justAdded) {
        return formalValue;
    }
    // clang-format off
    formalValue = visit(*value, utl::overload{
        [&](IntegralConstant const& constant) -> FormalValue {
            return constant.value();
        },
        [&](Parameter const& param) -> FormalValue { return Inevaluable{}; },
        [&](Value const& other) -> FormalValue { return Unexamined{}; }
    }); // clang-format on
    return formalValue;
}
