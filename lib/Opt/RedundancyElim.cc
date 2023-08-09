#include "Opt/RedundancyElim.h"

#include <iostream>
#include <map>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Print.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct Expression {
    Instruction* inst;

    explicit Expression(Instruction* inst);

    Type const* type() const { return inst->type(); }

    std::string name() const {
        return visit(*inst,
                     utl::overload{ [](ArithmeticInst const& inst) {
                                       return std::string(
                                           toString(inst.operation()));
                                   },
                                    [](Instruction const&) -> std::string {
                                        SC_UNREACHABLE();
                                    } }); // clang-format on
    }
};

/// Compares expressions lexicographically
struct ExprCompare {
    bool operator()(Expression const& a, Expression const& b) const;
};

struct ExprContext {
    size_t numOperands;
    utl::small_vector<Instruction*> instructions;
    utl::small_vector<Phi*> phis = {};

    utl::hashmap<BasicBlock*, utl::small_vector<Instruction*>> occurences;

    void gatherOccurences();
};

struct PREContext {
    using ExprMap = std::map<Expression, ExprContext, ExprCompare>;

    ir::Context& ctx;
    ir::Function& function;
    ExprMap exprMap;

    PREContext(ir::Context& irCtx, ir::Function& function):
        ctx(irCtx), function(function) {}

    bool run();

    void gatherExpressions();

    void phiInsertion();

    void rename();

    utl::hashset<Phi*> rename1(ExprContext& exprCtx);

    void rename2(ExprContext& exprCtx, utl::hashset<Phi*> setForRename2);

    UniquePtr<Phi> phiOperandFromRes(Phi* Z, size_t j);
};

} // namespace

bool opt::redundancyElim(ir::Context& irCtx, ir::Function& function) {
    return PREContext(irCtx, function).run();
}

bool PREContext::run() {
    splitCriticalEdges(ctx, function);
    gatherExpressions();
    phiInsertion();
    rename();
    //    for (auto&& [expr, exprCtx]: exprMap) {
    //        std::cout << "===================\n";
    //        for (auto* inst: exprCtx.instructions) {
    //            std::cout << *inst << std::endl;
    //        }
    //    }
    return false;
}

void PREContext::gatherExpressions() {
    for (auto& inst: function.instructions() | Filter<ArithmeticInst>) {
        Expression const expr{ &inst };
        auto& exprCtx = exprMap[expr];
        exprCtx.numOperands = inst.operands().size();
        exprCtx.instructions.push_back(&inst);
    }
}

static void merge(utl::hashset<BasicBlock*>& set, auto&& R) {
    for (auto* BB: R) {
        set.insert(BB);
    }
}

void PREContext::phiInsertion() {
    auto& DF = function.getOrComputeDomInfo().domFronts();
    auto IDF = DominanceInfo::computeIterDomFronts(DF);
    utl::hashmap<size_t, utl::hashset<BasicBlock*>> dfPhis;
    utl::hashmap<size_t, utl::hashmap<size_t, utl::hashset<BasicBlock*>>>
        varPhis;

    auto setVarPhisImpl =
        [&](auto setVarPhisImpl, Phi* phi, size_t i, size_t j) {
        auto& varPhis_ij = varPhis[i][j];
        if (varPhis_ij.contains(phi->parent())) {
            return;
        }
        varPhis_ij.insert(phi->parent());
        for (auto* arg: phi->operands()) {
            if (auto* argPhi = dyncast<Phi*>(arg)) {
                setVarPhisImpl(setVarPhisImpl, argPhi, i, j);
            }
        }
    };
    auto setVarPhis = [&](Phi* phi, size_t i, size_t j) {
        setVarPhisImpl(setVarPhisImpl, phi, i, j);
    };

    for (auto&& [i, exprValue]: exprMap | ranges::views::enumerate) {
        auto&& [expr, exprCtx] = exprValue;
        for (auto [i, inst]: exprCtx.instructions | ranges::views::enumerate) {
            // DF_phis[i] := DF_phis[i] ∪ DF+(inst)
            merge(dfPhis[i], IDF.find(inst->parent())->second);
            for (auto [j, V]: inst->operands() | ranges::views::enumerate) {
                if (auto* phi = dyncast<Phi*>(V)) {
                    setVarPhis(phi, i, j);
                }
            }
        }
    }

    for (auto&& [i, exprValue]: exprMap | ranges::views::enumerate) {
        auto&& [expr, exprCtx] = exprValue;
        auto& dfPhis_i = dfPhis[i];
        auto& varPhis_i = varPhis[i];
        for (size_t j = 0; j < exprCtx.numOperands; ++j) {
            merge(dfPhis_i, varPhis_i[j]);
        }
        for (auto* BB: dfPhis_i) {
            auto* phi = new Phi(expr.type(), utl::strcat(expr.name(), ".phi"));
            phi->setArguments(BB->predecessors() |
                              ranges::views::transform([&](auto* pred) {
                                  return PhiMapping(pred, nullptr);
                              }) |
                              ranges::to<utl::small_vector<PhiMapping>>);
            BB->insertPhi(phi);
            exprCtx.phis.push_back(phi);
        }
        exprCtx.gatherOccurences();
    }
}

/// \returns `true` iff the instruction \p A dominates the instruction \p B
static bool dominates(Instruction const* A,
                      Instruction const* B,
                      DominanceInfo const& domInfo) {
    auto& domSet = domInfo.domSet(A->parent());
    if (!domSet.contains(B->parent())) {
        return false;
    }
    if (A->parent() != B->parent()) {
        return true;
    }
    auto* BB = A->parent();
    auto* end = BB->end().to_address();
    while (A != end) {
        if (A == B) {
            return true;
        }
        A = A->next();
    }
    return false;
}

void PREContext::rename() {
    for (auto& [expr, exprCtx]: exprMap) {
        auto setForRename2 = rename1(exprCtx);
        rename2(exprCtx, std::move(setForRename2));
    }
}

/// \returns `true` iff the value \p A dominates the instruction \p B
static bool dominates(Value const* A,
                      Instruction const* B,
                      DominanceInfo const& domInfo) {
    if (auto* inst = dyncast<Instruction const*>(A)) {
        return dominates(inst, B, domInfo);
    }
    SC_ASSERT(isa<Constant>(A) || isa<Parameter>(A), "What else?");
    return true;
}

static bool isReal(Instruction* inst) { return !isa<Phi>(inst); }

utl::hashset<Phi*> PREContext::rename1(ExprContext& exprCtx) {
    size_t count = 0;
    utl::stack<Instruction*> stack;
    utl::hashmap<Instruction*, size_t> Class;
    auto assignNewClass = [&](Instruction* inst) {
        Class[inst] = count++;
        stack.push(inst);
    };
    auto replaceDef = [&](Instruction* Y, Instruction* X) {
        Class[Y] = Class[X];
        replaceValue(Y, X);
        Y->parent()->erase(Y);
    };
    utl::hashset<Phi*> setForRename2;
    auto& domInfo = function.getOrComputeDomInfo();
    domInfo.domTree().root()->traversePreorder([&](DomTree::Node const* node) {
        for (Instruction* Y: exprCtx.occurences[node->basicBlock()]) {
            while (!stack.empty() && !dominates(stack.top(), Y, domInfo)) {
                stack.pop();
            }
            if (isa<Phi>(Y)) {
                assignNewClass(Y);
            }
            else if (true /* Y is a real occurence */) {
                if (stack.empty()) {
                    assignNewClass(Y);
                    continue;
                }
                auto* X = stack.top();
                if (isReal(X)) {
                    if (ranges::equal(Y->operands(), X->operands())) {
                        replaceDef(Y, X);
                    }
                    else {
                        assignNewClass(Y);
                    }
                }
                else {
                    SC_ASSERT(isa<Phi>(X), "Must be a phi");
                    bool allYOperandsDominateX =
                        ranges::all_of(Y->operands(), [&](Value* operand) {
                            return dominates(operand, X, domInfo);
                        });
                    if (allYOperandsDominateX) {
                        replaceDef(Y, X);
                        setForRename2.insert(cast<Phi*>(X));
                    }
                    else {
                        assignNewClass(Y);
                    }
                }
            }
            else /* Y is a phi argument */ {
                if (stack.empty()) {
                    // replace(Y, <bottom>)
                }
                else {
                    auto* X = stack.top();
                    Class[Y] = Class[X];
                    replaceValue(Y, X);
                }
            }
        }
    });
    return setForRename2;
}

void PREContext::rename2(ExprContext& exprCtx,
                         utl::hashset<Phi*> setForRename2) {
    utl::hashset<Value*> processed;
    while (!setForRename2.empty()) {
        Phi* Z = *setForRename2.begin();
        setForRename2.erase(setForRename2.begin());
        for (auto [j, operand]: Z->operands() | ranges::views::enumerate) {
            if (processed.contains(operand)) {
                continue;
            }
            UniquePtr<Phi> Y = phiOperandFromRes(Z, j);
            auto* X = operand;
            if (!isa<Phi>(X)) {
                SC_ASSERT(isa<Instruction>(X), "Must be a real occurence");
                //                if (!ranges::equal()) {
                //
                //                }
            }
            else /* X is a Φ occurrence */ {
            }
        }
    }
}

UniquePtr<Phi> PREContext::phiOperandFromRes(Phi* Z, size_t j) {
    auto* BB = Z->parent();
    auto Q = ir::clone(ctx, Z);
    for (auto [index, v]: Z->operands() | ranges::views::enumerate) {
        if (auto* vPhi = dyncast<Phi*>(v)) {
            Q->setOperand(index, vPhi->operandAt(j));
        }
    }
    return Q;
}

Expression::Expression(Instruction* inst): inst(inst) {}

bool ExprCompare::operator()(Expression const& a, Expression const& b) const {
    // clang-format off
    bool const opLess = visit(*a.inst, *b.inst, utl::overload{
        [](ArithmeticInst const& a, ArithmeticInst const& b) {
            return utl::to_underlying(a.operation()) <
                   utl::to_underlying(b.operation());
        },
        [](Instruction const&, Instruction const&) -> bool { SC_UNREACHABLE(); }
    }); // clang-format on
    if (opLess) {
        return true;
    }
    return a.type() < b.type();
}

void ExprContext::gatherOccurences() {
    for (auto* phi: phis) {
        occurences[phi->parent()].push_back(phi);
    }
    for (auto* inst: instructions) {
        occurences[inst->parent()].push_back(inst);
    }
}
