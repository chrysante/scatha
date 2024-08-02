#include "Opt/ScalarEvolution.h"

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "IR/Type.h"

#include <iostream>

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

ScevOperation ScevArithmeticExpr::operation() const {
    using enum ScevOperation;
    return SC_MATCH (*this){ [](ScevAddExpr const&) { return Add; },
                             [](ScevMulExpr const&) { return Mul; } };
}

UniquePtr<ScevExpr> opt::clone(ScevExpr const& expr) {
    // clang-format off
    return SC_MATCH_R (UniquePtr<ScevExpr>, expr) {
        [](ScevConstExpr const& expr) {
            return allocate<ScevConstExpr>(expr.value());
        },
        [](ScevUnknownExpr const& expr) {
            return allocate<ScevUnknownExpr>(expr.value());
        },
        []<std::derived_from<ScevArithmeticExpr> Expr>(Expr const& expr) {
            return allocate<Expr>(clone(*expr.LHS()), clone(*expr.RHS()));
        }
    }; // clang-format on
}

static void write(std::ostream& str, ScevExpr const& expr);

static void writeImpl(std::ostream& str, ScevConstExpr const& expr) {
    str << expr.value().signedToString();
}

static void writeImpl(std::ostream& str, ScevUnknownExpr const& expr) {
    str << formatName(*expr.value());
}

static std::string_view toSpelling(ScevOperation op) {
    using enum ScevOperation;
    switch (op) {
    case Add:
        return "+";
    case Mul:
        return "*";
    }
}

static void writeImpl(std::ostream& str, ScevArithmeticExpr const& expr) {
    write(str, *expr.LHS());
    str << ", " << toSpelling(expr.operation()) << ", ";
    write(str, *expr.RHS());
}

static void write(std::ostream& str, ScevExpr const& expr) {
    visit(expr, [&](auto& expr) { writeImpl(str, expr); });
}

utl::vstreammanip<> opt::format(ScevExpr const& expr) {
    return [&](std::ostream& str) {
        str << "{ ";
        write(str, expr);
        str << " }";
    };
}

void opt::print(ScevExpr const& expr) { print(expr, std::cout); }

void opt::print(ScevExpr const& expr, std::ostream& str) {
    str << format(expr) << std::endl;
}

static std::optional<std::pair<BasicBlock*, BasicBlock*>>
    determinePreheaderAndLatch(LNFNode& loop) {
    auto* header = loop.basicBlock();
    if (header->numPredecessors() != 2) {
        /// Loop is not canonical
        return std::nullopt;
    }
    auto& info = loop.loopInfo();
    auto* predA = header->predecessor(0);
    auto* predB = header->predecessor(1);
    if (info.isLatch(predA)) {
        SC_ASSERT(!info.isInner(predB), "predB must be the preheader");
        return std::pair{ predB, predA };
    }
    SC_ASSERT(info.isLatch(predB), "predB must be the latch");
    SC_ASSERT(!info.isInner(predA), "predA must be the preheader");
    return std::pair{ predA, predB };
}

namespace {

struct PhiInfo {
    Phi* phi;
    Value* phOperand;
    Value* latchOperand;
};

struct ScevCtx {
    Context& ctx;
    LNFNode& loop;
    LoopInfo& info = loop.loopInfo();

    void run();

    UniquePtr<ScevNullaryExpr> getNullary(Value& value);
    UniquePtr<ScevExpr> getOther(Value& value);
    UniquePtr<ScevExpr> findScevExprImpl(PhiInfo, Value&);
    UniquePtr<ScevExpr> findScevExprImpl(PhiInfo, IntegralConstant& value);
    UniquePtr<ScevExpr> findScevExprImpl(PhiInfo, Parameter& param);
    UniquePtr<ScevExpr> findScevExprImpl(PhiInfo phiInfo, ArithmeticInst& inst);
    UniquePtr<ScevExpr> findScevExpr(PhiInfo phiInfo);
};

} // namespace

void ScevCtx::run() {
    auto* header = loop.basicBlock();
    auto PL = determinePreheaderAndLatch(loop);
    if (!PL) {
        return;
    }
    auto [preheader, latch] = *PL;
    utl::small_vector<PhiInfo> worklist;
    for (auto& phi: header->phiNodes()) {
        if (isa<IntegralType>(phi.type())) {
            worklist.push_back({ .phi = &phi,
                                 .phOperand = phi.operandOf(preheader),
                                 .latchOperand = phi.operandOf(latch) });
        }
    }
    while (true) {
        bool any = false;
        for (auto itr = worklist.begin(); itr != worklist.end();) {
            auto phiInfo = *itr;
            auto expr = findScevExpr(phiInfo);
            if (!expr) {
                ++itr;
                continue;
            }
            itr = worklist.erase(itr);
            any = true;
            info.setScevExpr(phiInfo.phi, std::move(expr));
        }
        if (!any) {
            break;
        }
    }
}

UniquePtr<ScevNullaryExpr> ScevCtx::getNullary(Value& value) {
    SC_ASSERT(isa<IntegralType>(value.type()), "");
    if (auto* C = dyncast<IntegralConstant*>(&value)) {
        return allocate<ScevConstExpr>(C->value());
    }
    if (auto* inst = dyncast<Instruction*>(&value);
        inst && info.isInner(inst->parent()))
    {
        return nullptr;
    }
    return allocate<ScevUnknownExpr>(&value);
}

UniquePtr<ScevExpr> ScevCtx::getOther(Value& value) {
    if (auto expr = getNullary(value)) {
        return expr;
    }
    auto* inst = dyncast<Instruction*>(&value);
    if (!inst) {
        return nullptr;
    }
    if (auto* expr = info.getScevExpr(inst)) {
        return clone(*expr);
    }
    return nullptr;
}

UniquePtr<ScevExpr> ScevCtx::findScevExprImpl(PhiInfo, Value&) {
    return nullptr;
}

UniquePtr<ScevExpr> ScevCtx::findScevExprImpl(PhiInfo,
                                              IntegralConstant& value) {
    return getNullary(value);
}

UniquePtr<ScevExpr> ScevCtx::findScevExprImpl(PhiInfo, Parameter& param) {
    return getNullary(param);
}

UniquePtr<ScevExpr> ScevCtx::findScevExprImpl(PhiInfo phiInfo,
                                              ArithmeticInst& inst) {
    auto* LHS = inst.lhs();
    auto* RHS = inst.rhs();
    if (LHS != phiInfo.phi) {
        return nullptr;
    }
    auto base = getNullary(*phiInfo.phOperand);
    SC_ASSERT(base, "Must always have a corresponding scev expr");
    auto inc = getOther(*RHS);
    if (!inc) {
        return nullptr;
    }
    using enum ArithmeticOperation;
    switch (inst.operation()) {
    case Add:
        return allocate<ScevAddExpr>(std::move(base), std::move(inc));
    case Mul:
        return allocate<ScevMulExpr>(std::move(base), std::move(inc));
    default:
        return nullptr;
    }
}

UniquePtr<ScevExpr> ScevCtx::findScevExpr(PhiInfo phiInfo) {
    SC_ASSERT(isa<IntegralType>(phiInfo.phi->type()), "");
    return visit(*phiInfo.latchOperand,
                 [&](auto& value) { return findScevExprImpl(phiInfo, value); });
}

void opt::scev(Context& ctx, LNFNode& loop) { ScevCtx{ ctx, loop }.run(); }

SC_REGISTER_LOOP_PASS([](Context& ctx, LNFNode& loop) {
    scev(ctx, loop);
    return false;
}, "scev", PassCategory::Analysis, {});

static bool printScevExprsPass(Context&, LNFNode& loop) {
    auto& info = loop.loopInfo();
    for (auto& [inst, expr]: info.getScevExprMap()) {
        std::cout << formatName(*inst) << ": " << format(*expr) << std::endl;
    }
    return false;
}

SC_REGISTER_LOOP_PASS(printScevExprsPass, "print-scev", PassCategory::Other,
                      {});
