#include "Opt/Passes.h"

#include <deque>
#include <variant>

#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/ForeignFunctionDecl.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/PassRegistry.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

/// Implemented with help from:
/// https://www.cs.utexas.edu/users/lin/cs380c/wegman.pdf

SC_REGISTER_PASS(opt::propagateConstants,
                 "propagateconst",
                 PassCategory::Simplification);

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
    std::size_t operator()(FlowEdge const& e) const {
        return utl::hash_combine(e.origin, e.dest);
    }
};

namespace {

enum class Inevaluable {};

/// Supremum, evaluation
enum class Unexamined {};

using FormalValue = std::variant<Unexamined, Inevaluable, APInt, APFloat>;

bool isUnexamined(FormalValue const& value) { return value.index() == 0; }

[[maybe_unused]] bool isInevaluable(FormalValue const& value) {
    return value.index() == 1;
}

bool isConstant(FormalValue const& value) {
    return value.index() == 2 /* APInt */ || value.index() == 3 /* APFloat */;
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

FormalValue infimum(auto&& range) {
    return ranges::accumulate(range,
                              FormalValue{ Unexamined{} },
                              [](FormalValue const& a, FormalValue const& b) {
        return infimum(a, b);
    });
}

struct SCCPContext {
    explicit SCCPContext(Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    bool apply();

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

    bool controlledByConstant(TerminatorInst const& terminator);

    FormalValue evaluateConversion(Conversion conv,
                                   ArithmeticType const* targetType,
                                   FormalValue operand);

    FormalValue evaluateArithmetic(ArithmeticOperation operation,
                                   FormalValue const& lhs,
                                   FormalValue const& rhs);

    FormalValue evaluateUnaryArithmetic(UnaryArithmeticOperation operation,
                                        FormalValue const& operand);

    FormalValue evaluateComparison(CompareOperation operation,
                                   FormalValue const& lhs,
                                   FormalValue const& rhs);

    FormalValue evaluateCall(Value const* target,
                             std::span<FormalValue const> args);

    bool isExpression(Instruction const* inst) const {
        return inst != nullptr && !isa<Phi>(inst) && !isa<TerminatorInst>(inst);
    }

    /// Unexamined edges are not executable
    bool isExecutable(FlowEdge const& e) {
        return execMap.insert({ e, false }).first->second;
    }

    void setExecutable(FlowEdge const& e, bool value) {
        execMap.insert_or_assign(e, value);
    }

    FormalValue formalValue(Value* value);

    void setFormalValue(Value* value, FormalValue formalValue) {
        formalValues.insert_or_assign(value, formalValue);
    }

    Context& irCtx;
    Function& function;

    std::deque<FlowEdge> flowWorklist;
    std::deque<UseEdge> useWorklist;
    utl::hashmap<Value*, FormalValue> formalValues;
    utl::hashmap<FlowEdge, bool> execMap;
};

} // namespace

bool opt::propagateConstants(ir::Context& context, ir::Function& function) {
    SCCPContext ctx(context, function);
    bool const result = ctx.run();
    assertInvariants(context, function);
    return result;
}

bool SCCPContext::run() {
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
    return apply();
}

bool SCCPContext::apply() {
    utl::small_vector<Instruction*> replacedInstructions;
    for (auto [value, latticeElement]: formalValues) {
        if (!isConstant(latticeElement)) {
            continue;
        }
        if (isa<Constant>(value)) {
            continue;
        }
        SC_ASSERT(isa<Instruction>(value), "We can only replace instructions");
        // clang-format off
        Value* const newValue = std::visit(utl::overload{
            [&](APInt const& constant) -> Value* {
                return irCtx.intConstant(constant);
            },
            [&](APFloat const& constant) -> Value* {
                return irCtx.floatConstant(constant);
            },
            [](auto const&) -> Value* { SC_UNREACHABLE(); }
        }, latticeElement); // clang-format on
        value->replaceAllUsesWith(newValue);
        replacedInstructions.push_back(cast<Instruction*>(value));
    }
    for (auto* inst: replacedInstructions) {
        if (!inst->users().empty()) {
            continue;
        }
        inst->parent()->erase(inst);
    }
    return !replacedInstructions.empty();
}

void SCCPContext::processFlowEdge(FlowEdge edge) {
    if (isExecutable(edge)) {
        return;
    }
    setExecutable(edge, true);
    auto const [origin, dest] = edge;
    for (auto& phi: dest->phiNodes()) {
        visitPhi(phi);
    }
    if (dest->isEntry() || numIncomingExecutableEdges(*dest) == 1) {
        visitExpressions(*dest);
    }
    if (dest->successors().size() == 1) {
        flowWorklist.push_back({ dest, dest->successors().front() });
    }
    else if (auto* term = dest->terminator(); controlledByConstant(*term)) {
        FormalValue const fv =
            isa<Branch>(term) ? formalValue(cast<Branch*>(term)->condition()) :
                                Inevaluable{};
        processTerminator(fv, *term);
    }
}

void SCCPContext::processUseEdge(UseEdge edge) {
    // clang-format off
    visit(*edge.user, utl::overload{
        [&](Phi& phi) { visitPhi(phi); },
        [&](Instruction& inst) {
            if (basicBlockIsExecutable(*inst.parent())) {
                visitExpression(inst);
            }
        },
        [](User const&) {}
    }); // clang-format on
}

void SCCPContext::visitPhi(Phi& phi) {
    auto formalArgs =
        phi.arguments() |
        ranges::views::transform([&](PhiMapping arg) -> FormalValue {
            return isExecutable({ arg.pred, phi.parent() }) ?
                       formalValue(arg.value) :
                       Unexamined{};
        });
    FormalValue const value = infimum(formalArgs);
    if (value == formalValue(&phi)) {
        return;
    }
    setFormalValue(&phi, value);
    notifyUsers(phi);
}

void SCCPContext::visitExpressions(BasicBlock& basicBlock) {
    for (auto& inst: basicBlock) {
        if (!isExpression(&inst)) {
            continue;
        }
        visitExpression(inst);
    }
}

void SCCPContext::visitExpression(Instruction& inst) {
    SC_EXPECT(isExpression(&inst));
    FormalValue const oldValue = formalValue(&inst);
    // clang-format off
    FormalValue const value = SC_MATCH (inst) {
        [&](ConversionInst& inst) {
            return evaluateConversion(inst.conversion(),
                                      inst.type(),
                                      formalValue(inst.operand()));
        },
        [&](ArithmeticInst& inst) {
            return evaluateArithmetic(inst.operation(),
                                      formalValue(inst.lhs()),
                                      formalValue(inst.rhs()));
        },
        [&](UnaryArithmeticInst& inst) {
            return evaluateUnaryArithmetic(inst.operation(),
                                           formalValue(inst.operand()));
        },
        [&](CompareInst& inst) {
            return evaluateComparison(inst.operation(),
                                      formalValue(inst.lhs()),
                                      formalValue(inst.rhs()));
        },
        [&](Call& call) {
            auto args = call.arguments() |
                        ranges::views::transform([&](Value* arg) {
                            return formalValue(arg);
                        }) |
            ToSmallVector<>;
            return evaluateCall(call.function(), args);
        },
        [&](Select& inst) {
            return infimum(formalValue(inst.thenValue()),
                           formalValue(inst.elseValue()));
        },
        [&](Instruction const&) -> FormalValue { return Inevaluable{}; }
    }; // clang-format on
    if (value == oldValue) {
        return;
    }
    setFormalValue(&inst, value);
    notifyUsers(inst);
}

void SCCPContext::notifyUsers(Value& value) {
    for (auto* user: value.users()) {
        if (auto* phi = dyncast<Phi*>(user)) {
            useWorklist.push_back({ &value, phi });
        }
        else if (isExpression(dyncast<Instruction const*>(user))) {
            useWorklist.push_back({ &value, user });
        }
        else if (auto* term = dyncast<TerminatorInst*>(user)) {
            processTerminator(formalValue(&value), *term);
        }
    }
}

void SCCPContext::processTerminator(FormalValue const& value,
                                    TerminatorInst& inst) {
    // clang-format off
    std::visit(utl::overload{
        [&](Unexamined) {
            SC_UNREACHABLE();
        },
        [&](APInt const& constant) {
            addSingleEdge(constant, inst);
        },
        [&](APFloat const& constant) {
            SC_ASSERT(isa<Return>(inst),
                      "Float can atmost control return instructions");
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

void SCCPContext::addSingleEdge(APInt const& constant, TerminatorInst& inst) {
    // clang-format off
    visit(inst, utl::overload{
        [&](Goto& gt)   {
            flowWorklist.push_back({ inst.parent(), gt.target() });
        },
        [&](Branch& br) {
            SC_ASSERT(constant == 0 || constant == 1,
                      "Boolean constant must be 0 or 1");
            BasicBlock* const origin = br.parent();
            auto const index = 1 - constant.to<ssize_t>();
            BasicBlock* const target = br.targets()[index];
            FlowEdge const edge = { origin, target };
            setExecutable(edge, false);
            flowWorklist.push_back(edge);
        },
        [&](Return& inst) {}
    }); // clang-format on
}

bool SCCPContext::basicBlockIsExecutable(BasicBlock& bb) {
    if (bb.isEntry()) {
        return true;
    }
    for (auto* pred: bb.predecessors()) {
        if (isExecutable({ pred, &bb })) {
            return true;
        }
    }
    return false;
}

size_t SCCPContext::numIncomingExecutableEdges(BasicBlock& basicBlock) {
    size_t result = 0;
    for (auto* pred: basicBlock.predecessors()) {
        result += isExecutable({ pred, &basicBlock });
    }
    return result;
}

bool SCCPContext::controlledByConstant(TerminatorInst const& terminator) {
    // clang-format off
    return visit(terminator, utl::overload{
        [](Goto const&) { return true; },
        [](Branch const& br) { return isa<IntegralConstant>(br.condition()); },
        [](Return const&) { return true; }
    }); // clang-format on
}

template <typename T>
concept FormalConstant = std::same_as<T, APInt> || std::same_as<T, APFloat>;

FormalValue SCCPContext::evaluateConversion(Conversion conv,
                                            ArithmeticType const* targetType,
                                            FormalValue operand) {
    if (!isConstant(operand)) {
        return operand;
    }
    switch (conv) {
    case Conversion::Zext: {
        APInt value = std::get<APInt>(operand);
        return value.zext(targetType->bitwidth());
    }
    case Conversion::Sext: {
        APInt value = std::get<APInt>(operand);
        return value.sext(targetType->bitwidth());
    }
    case Conversion::Trunc: {
        APInt value = std::get<APInt>(operand);
        /// `APInt::zext` also handles truncation.
        return value.zext(targetType->bitwidth());
    }
    case Conversion::Fext: {
        APFloat value = std::get<APFloat>(operand);
        SC_ASSERT(value.precision() == APFloatPrec::Single,
                  "Can only extend single precision floats");
        SC_ASSERT(targetType->bitwidth() == 64,
                  "Can only extend to 64 bit floats");
        return APFloat(value.to<float>(), APFloatPrec::Double);
    }
    case Conversion::Ftrunc: {
        APFloat value = std::get<APFloat>(operand);
        SC_ASSERT(value.precision() == APFloatPrec::Double,
                  "Can only truncate double precision floats");
        SC_ASSERT(targetType->bitwidth() == 32,
                  "Can only truncate to 32 bit floats");
        return APFloat(value.to<double>(), APFloatPrec::Single);
    }
    case Conversion::UtoF:
        return valuecast<APFloat>(std::get<APInt>(operand),
                                  targetType->bitwidth());

    case Conversion::StoF:
        return signedValuecast<APFloat>(std::get<APInt>(operand),
                                        targetType->bitwidth());

    case Conversion::FtoU:
        return valuecast<APInt>(std::get<APFloat>(operand),
                                targetType->bitwidth());

    case Conversion::FtoS:
        return signedValuecast<APInt>(std::get<APFloat>(operand),
                                      targetType->bitwidth());

    case Conversion::Bitcast: {
        if (std::holds_alternative<APInt>(operand)) {
            return bitcast<APFloat>(std::get<APInt>(operand));
        }
        else {
            return bitcast<APInt>(std::get<APFloat>(operand));
        }
    }
    case Conversion::_count:
        SC_UNREACHABLE();
    }
}

FormalValue SCCPContext::evaluateArithmetic(ArithmeticOperation operation,
                                            FormalValue const& lhs,
                                            FormalValue const& rhs) {
    switch (operation) {
    case ArithmeticOperation::Add:
        [[fallthrough]];
    case ArithmeticOperation::Sub:
        [[fallthrough]];
    case ArithmeticOperation::FAdd:
        [[fallthrough]];
    case ArithmeticOperation::FSub:
        // clang-format off
        return std::visit(utl::overload{
            [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Add: return add(lhs, rhs);
                case ArithmeticOperation::Sub: return sub(lhs, rhs);
                default: SC_UNREACHABLE();
                }
            },
            [&](APFloat const& lhs, APFloat const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::FAdd: return add(lhs, rhs);
                case ArithmeticOperation::FSub: return sub(lhs, rhs);
                default: SC_UNREACHABLE();
                }
            },
            [](Inevaluable, auto const&) -> FormalValue { return Inevaluable{}; },
            [](auto const&, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Unexamined{}; },
        }, lhs, rhs); // clang-format on
    case ArithmeticOperation::Mul:
        [[fallthrough]];
    case ArithmeticOperation::SDiv:
        [[fallthrough]];
    case ArithmeticOperation::UDiv:
        [[fallthrough]];
    case ArithmeticOperation::SRem:
        [[fallthrough]];
    case ArithmeticOperation::URem:
        [[fallthrough]];
    case ArithmeticOperation::FMul:
        [[fallthrough]];
    case ArithmeticOperation::FDiv:
        [[fallthrough]];
    case ArithmeticOperation::LShL:
        [[fallthrough]];
    case ArithmeticOperation::LShR:
        [[fallthrough]];
    case ArithmeticOperation::AShL:
        [[fallthrough]];
    case ArithmeticOperation::AShR:
        [[fallthrough]];
    case ArithmeticOperation::And:
        [[fallthrough]];
    case ArithmeticOperation::Or:
        [[fallthrough]];
    case ArithmeticOperation::XOr:
        // clang-format off
        return std::visit(utl::overload{
            [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::Mul: return mul(lhs, rhs);
                case ArithmeticOperation::SDiv: return sdiv(lhs, rhs);
                case ArithmeticOperation::UDiv: return udiv(lhs, rhs);
                case ArithmeticOperation::SRem: return srem(lhs, rhs);
                case ArithmeticOperation::URem: return urem(lhs, rhs);
                case ArithmeticOperation::LShL:
                    return lshl(lhs, utl::narrow_cast<int>(rhs.to<u64>()));
                case ArithmeticOperation::LShR:
                    return lshr(lhs, utl::narrow_cast<int>(rhs.to<u64>()));
                case ArithmeticOperation::AShL:
                    return ashl(lhs, utl::narrow_cast<int>(rhs.to<u64>()));
                case ArithmeticOperation::AShR:
                    return ashr(lhs, utl::narrow_cast<int>(rhs.to<u64>()));
                case ArithmeticOperation::And: return btwand(lhs, rhs);
                case ArithmeticOperation::Or: return btwor(lhs, rhs);
                case ArithmeticOperation::XOr: return btwxor(lhs, rhs);
                default: SC_UNREACHABLE();
                }
            },
            [&](APFloat const& lhs, APFloat const& rhs) -> FormalValue {
                switch (operation) {
                case ArithmeticOperation::FMul: return mul(lhs, rhs);
                case ArithmeticOperation::FDiv: return div(lhs, rhs);
                default: SC_UNREACHABLE();
                }
            },
            []<typename T>(APInt const& lhs, T const& rhs) -> FormalValue {
                /// Here and in the case below we still have optimization
                /// opportunities, e.g. when we have:
                /// - `0 & <ineval>          ==>    0`
                /// - `<uintmax> | <ineval>  ==>    <uintmax>`
                /// - `<ineval> * 0          ==>    0`
                /// - `<ineval> / 0          ==>    undef`
                /// - `<ineval> % 0          ==>    undef`
                return T{};
            },
            []<typename T>(T const& lhs, APInt const& rhs) -> FormalValue {
                return T{};
            },
            [](Inevaluable, Unexamined)  -> FormalValue { return Inevaluable{}; },
            [](Unexamined,  Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
            [](APInt const&, APFloat const&) -> FormalValue { SC_UNREACHABLE(); },
            [](APFloat const&, APInt const&) -> FormalValue { SC_UNREACHABLE(); }
        }, lhs, rhs); // clang-format on

    default:
        return Inevaluable{};
    }
}

FormalValue SCCPContext::evaluateUnaryArithmetic(
    UnaryArithmeticOperation operation, FormalValue const& operand) {
    // clang-format off
    return std::visit(utl::overload{
        [&](APInt const& operand) -> FormalValue {
            switch (operation) {
            case UnaryArithmeticOperation::BitwiseNot: return btwnot(operand);
            case UnaryArithmeticOperation::LogicalNot:
                SC_ASSERT(operand == 0 || operand == 1,
                          "Operand must be boolean");
                return sub(APInt(1, operand.bitwidth()), operand);
            case UnaryArithmeticOperation::Negate:
                return negate(operand);
            case UnaryArithmeticOperation::_count: SC_UNREACHABLE();
            }
        },
        [&](APFloat const& operand) -> FormalValue {
            switch (operation) {
            case UnaryArithmeticOperation::BitwiseNot: [[fallthrough]];
            case UnaryArithmeticOperation::LogicalNot: [[fallthrough]];
            case UnaryArithmeticOperation::Negate: [[fallthrough]];
            case UnaryArithmeticOperation::_count: SC_UNREACHABLE();
            }
        },
        [](auto const&) -> FormalValue { return Inevaluable{}; },
    }, operand); // clang-format on
}

FormalValue SCCPContext::evaluateComparison(CompareOperation operation,
                                            FormalValue const& lhs,
                                            FormalValue const& rhs) {
    // clang-format off
    return std::visit(utl::overload{
        [&](APInt const& lhs, APInt const& rhs) -> FormalValue {
            switch (operation) {
            case CompareOperation::Less:
                return APInt(scmp(lhs, rhs) <  0, 1);
            case CompareOperation::LessEq:
                return APInt(scmp(lhs, rhs) <= 0, 1);
            case CompareOperation::Greater:
                return APInt(scmp(lhs, rhs) >  0, 1);
            case CompareOperation::GreaterEq:
                return APInt(scmp(lhs, rhs) >= 0, 1);
            case CompareOperation::Equal:
                return APInt(scmp(lhs, rhs) == 0, 1);
            case CompareOperation::NotEqual:
                return APInt(scmp(lhs, rhs) != 0, 1);
            case CompareOperation::_count: SC_UNREACHABLE();
            }
        },
        [&](APFloat const& lhs, APFloat const& rhs) -> FormalValue {
            switch (operation) {
            case CompareOperation::Less:      return APInt(lhs <  rhs, 1);
            case CompareOperation::LessEq:    return APInt(lhs <= rhs, 1);
            case CompareOperation::Greater:   return APInt(lhs >  rhs, 1);
            case CompareOperation::GreaterEq: return APInt(lhs >= rhs, 1);
            case CompareOperation::Equal:     return APInt(lhs == rhs, 1);
            case CompareOperation::NotEqual:  return APInt(lhs != rhs, 1);
            case CompareOperation::_count: SC_UNREACHABLE();
            }
        },
        [](auto const&, auto const&) -> FormalValue { return Inevaluable{}; },
    }, lhs, rhs); // clang-format on
}

FormalValue SCCPContext::evaluateCall(Value const* target,
                                      std::span<FormalValue const> args) {
    /// Right now we can atmost evaluate certain builtin functions.
    auto* extFn = dyncast<ForeignFunction const*>(target);
    if (!extFn) {
        return Inevaluable{};
    }
    /// We need all arguments be constant to evaluate this.
    if (!ranges::all_of(args, [&](FormalValue const& value) {
            return isConstant(value);
        }))
    {
        return Inevaluable{};
    }
    auto index = getBuiltinIndex(extFn->name());
    if (!index) {
        return Inevaluable{};
    }
    switch (static_cast<svm::Builtin>(*index)) {
    case svm::Builtin::abs_f64:
        return abs(std::get<APFloat>(args[0]));
    case svm::Builtin::exp_f64:
        return exp(std::get<APFloat>(args[0]));
    case svm::Builtin::exp2_f64:
        return exp2(std::get<APFloat>(args[0]));
    case svm::Builtin::exp10_f64:
        return exp10(std::get<APFloat>(args[0]));
    case svm::Builtin::log_f64:
        return log(std::get<APFloat>(args[0]));
    case svm::Builtin::log2_f64:
        return log2(std::get<APFloat>(args[0]));
    case svm::Builtin::log10_f64:
        return log10(std::get<APFloat>(args[0]));
    case svm::Builtin::pow_f64:
        return pow(std::get<APFloat>(args[0]), std::get<APFloat>(args[1]));
    case svm::Builtin::sqrt_f64:
        return sqrt(std::get<APFloat>(args[0]));
    case svm::Builtin::cbrt_f64:
        return cbrt(std::get<APFloat>(args[0]));
    case svm::Builtin::hypot_f64:
        return hypot(std::get<APFloat>(args[0]), std::get<APFloat>(args[1]));
    case svm::Builtin::sin_f64:
        return sin(std::get<APFloat>(args[0]));
    case svm::Builtin::cos_f64:
        return cos(std::get<APFloat>(args[0]));
    case svm::Builtin::tan_f64:
        return tan(std::get<APFloat>(args[0]));
    case svm::Builtin::asin_f64:
        return asin(std::get<APFloat>(args[0]));
    case svm::Builtin::acos_f64:
        return acos(std::get<APFloat>(args[0]));
    case svm::Builtin::atan_f64:
        return atan(std::get<APFloat>(args[0]));

    default:
        return Inevaluable{};
    }
}

FormalValue SCCPContext::formalValue(Value* value) {
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
        [&](FloatingPointConstant const& constant) -> FormalValue {
            return constant.value();
        },
        [&](Parameter const& param) -> FormalValue { return Inevaluable{}; },
        [&](Value const& other) -> FormalValue { return Unexamined{}; }
    }); // clang-format on
    return formalValue;
}
