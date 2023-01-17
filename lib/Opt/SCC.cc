#include "Opt/SCC.h"

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/variant.hpp>

#include "Common/APInt.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

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
enum class Varying{};

using FormalValue = utl::variant<Varying, Inevaluable, APInt>;

bool isVarying(FormalValue const& value) { return value.index() == 0; }

bool isInevaluable(FormalValue const& value) { return value.index() == 1; }

bool isConstant(FormalValue const& value) { return value.index() == 2; }

/// One context object is created per analyzed function.
struct SCCContext {
    explicit SCCContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}

    void run();

    void processFlowEdge(FlowEdge edge);
    
    void visitPhi(Phi& phi);

    void visitExpr(Instruction& inst);
    
    void processTerminator(FormalValue const& value, TerminatorInst& inst);
    
    void addEdge(APInt const& constant, TerminatorInst& inst);
    
    void processUseEdge(UseEdge edge);

    bool basicBlockIsExecutable(BasicBlock& basicBlock);
    
    FormalValue evaluateArithmetic(ArithmeticOperation operation, FormalValue const& lhs, FormalValue const& rhs);
    
    FormalValue evaluateComparison(CompareOperation operation, FormalValue const& lhs, FormalValue const& rhs);
    
    bool isExpression(Instruction const* inst) const { return inst != nullptr && !isa<Phi>(inst) && !isa<TerminatorInst>(inst); }
    
    bool executable(FlowEdge const& e) { return execMap.insert({ e, false }).first->second; }

    void setExecutable(FlowEdge const& e, bool value) { execMap.insert_or_assign(e, value); }

    FormalValue formalValue(Value const* value) {
        auto const [itr, justAdded] = formalValues.insert({ value, Varying{} });
        auto&& [key, formalValue] = *itr;
        if (!justAdded) {
            return formalValue;
        }
        // clang-format off
        formalValue = visit(*value, utl::overload{
            [&](IntegralConstant const& constant) -> FormalValue {
                return constant.value();
            },
            [&](Value const& other) -> FormalValue { return Varying{}; }
        }); // clang-format on
        return formalValue;
    }
    
    void setFormalValue(Value const* value, FormalValue formalValue) {
        formalValues.insert_or_assign(value, formalValue);
    }
    
    Context& irCtx;
    Function& function;
    
    utl::vector<FlowEdge> flowWorklist;
    utl::vector<UseEdge> useWorklist;
    utl::hashmap<Value const*, FormalValue> formalValues;
    utl::hashmap<FlowEdge, bool> execMap;
};

} // namespace

void opt::scc(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        SCCContext ctx(context, function);
        ctx.run();
    }
    assertInvariants(context, mod);
}

void SCCContext::run() {
    auto& entry = function.entry();
    // Same code as in processFlowEdge, extract to function
    for (auto& inst: entry.instructions) {
        if (!isExpression(&inst)) {
            continue;
        }
        visitExpr(inst);
    }
    while (!flowWorklist.empty() || !useWorklist.empty()) {
        if (!flowWorklist.empty()) {
            FlowEdge const edge = flowWorklist.back();
            flowWorklist.pop_back();
            processFlowEdge(edge);
        }
        if (!useWorklist.empty()) {
            UseEdge const edge = useWorklist.back();
            useWorklist.pop_back();
            processUseEdge(edge);
        }
    }
}

void SCCContext::processFlowEdge(FlowEdge edge) {
    if (executable(edge)) {
        return;
    }
    setExecutable(edge, true);
    for (auto& phi: edge.dest->phis()) {
        visitPhi(phi);
    }
    size_t numIncomingExecEdges = 0;
    for (auto* pred: edge.dest->predecessors) {
        numIncomingExecEdges += executable({ pred, edge.dest });
        if (numIncomingExecEdges > 1) { break; }
    }
    if (numIncomingExecEdges == 1) {
        for (auto& inst: edge.dest->instructions) {
            if (!isExpression(&inst)) {
                continue;
            }
            visitExpr(inst);
        }
    }
    if (edge.dest->successors().size() == 1) {
        flowWorklist.push_back({ edge.dest, edge.dest->successors().front() });
    }
}

void SCCContext::visitPhi(Phi& phi) {
    
}

#include <iostream>
#include "IR/Print.h"

static std::ostream& operator<<(std::ostream& str, FormalValue const& v) {
    return v.visit(utl::overload {
        [&](Varying) -> auto& { return str << "Varying"; },
        [&](Inevaluable) -> auto& { return str << "Inevaluable"; },
        [&](APInt const& constant) -> auto& { return str << constant; }
    });
}

void SCCContext::visitExpr(Instruction& inst) {
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
    std::cout << inst << " -> " << value << std::endl;
    if (value == oldValue) {
        return;
    }
    Value* possibleReplacement = &inst;
    if (isConstant(value)) {
        APInt const& constant = value.get<APInt>();
        possibleReplacement = irCtx.integralConstant(constant, cast<IntegralType const*>(inst.type())->bitWidth());
        replaceValue(&inst, possibleReplacement);
        if (inst.users().empty()) {
            inst.parent()->instructions.erase(&inst);
            inst.clearOperands();
        }
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
}

void SCCContext::processTerminator(FormalValue const& value, TerminatorInst& inst) {
    // clang-format off
    utl::visit(utl::overload{
        [&](Inevaluable) {
            flowWorklist.insert(flowWorklist.end(), utl::transform(inst.targets(), [&](BasicBlock* target) { return FlowEdge{ inst.parent(), target }; }));
        },
        [&](Varying) {
            SC_UNREACHABLE();
        },
        [&](APInt const& constant) {
            addEdge(constant, inst);
        },
    }, value);// clang-format on
}

void SCCContext::addEdge(APInt const& constant, TerminatorInst& inst) {
    // clang-format off
    visit(inst, utl::overload{
        [&](Goto& gt)   {
            flowWorklist.push_back({ inst.parent(), gt.target() });
        },
        [&](Branch& br) {
            SC_ASSERT(constant == 0 || constant == 1, "Boolean constant must be 0 or 1");
            BasicBlock* const origin = br.parent();
            bool const index = static_cast<bool>(constant);
            BasicBlock* const target = br.targets()[index];
            BasicBlock* const staleTarget = br.targets()[!index];
            flowWorklist.push_back({ origin, target });
            auto const pos = origin->instructions.erase(&br);
            staleTarget->removePredecessor(origin);
            br.clearOperands();
            origin->addInstruction(pos, new Goto(irCtx, target));
        },
        [&](Return& inst) {},
        [](TerminatorInst const&) { SC_UNREACHABLE(); }
    }); // clang-format on
}

bool SCCContext::basicBlockIsExecutable(BasicBlock& bb) {
    for (auto* pred: bb.predecessors) {
        if (executable({ pred, &bb })) {
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
                visitExpr(inst);
            }
        },
        [](TerminatorInst const&) {},
        [](Value const&) { SC_UNREACHABLE(); }
    }); // clang-format on
}

FormalValue SCCContext::evaluateArithmetic(ArithmeticOperation operation, FormalValue const& lhs, FormalValue const& rhs) {
    switch (operation) {
    case ArithmeticOperation::Add:
        return utl::visit(utl::overload{
            [](APInt const& lhs, APInt const& rhs) -> FormalValue {
                return lhs + rhs;
            },
            [](Inevaluable, auto const&) -> FormalValue { return Inevaluable{}; },
            [](auto const&, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Varying{}; },
        }, lhs, rhs);
        // Sub just like that
    case ArithmeticOperation::Mul:
        return utl::visit(utl::overload{
            [](APInt const& lhs, APInt const& rhs) -> FormalValue {
                return lhs * rhs;
            },
            []<typename T>(APInt const& lhs, T const& rhs) -> FormalValue {
                if (lhs == 0) {
                    return 0;
                }
                return T{};
            },
            []<typename T>(T const& lhs, APInt const& rhs) -> FormalValue {
                if (rhs == 0) {
                    return 0;
                }
                return T{};
            },
            [](Inevaluable, Varying)     -> FormalValue { return Inevaluable{}; },
            [](Varying,     Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](Inevaluable, Inevaluable) -> FormalValue { return Inevaluable{}; },
            [](auto const&, auto const&) -> FormalValue { return Varying{}; },
        }, lhs, rhs);
        // Div also very similar
        
        
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
        [](auto const&, auto const&) -> FormalValue { return Varying{}; },
    }, lhs, rhs);
}
