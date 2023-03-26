#include "Opt/TailRecElim.h"

#include <utl/variant.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct RetBase {
    Return* retInst;
};

struct DirectReturn: RetBase {
    Call* call;
};

struct AccumulatedReturn: RetBase {
    ArithmeticInst* accInst;
    Call* call;
    Value* otherAccArg;
};

struct DirectPhiReturn: RetBase {
    Phi* phi;
    BasicBlock* constantPred;
    Value* constant;
    BasicBlock* callPred;
    Call* call;
};

struct AccumulatedPhiReturn: RetBase {
    Phi* phi;
    BasicBlock* constantPred;
    Value* constant;
    BasicBlock* accPred;
    ArithmeticInst* accInst;
    Call* call;
    Value* otherAccArg;
};

using ViableReturn = utl::variant<DirectReturn,
                                  AccumulatedReturn,
                                  DirectPhiReturn,
                                  AccumulatedPhiReturn>;

struct TREContext {
    TREContext(Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    void gather();

    void generateLoopHeader();

    void rewrite(DirectReturn info);

    void rewrite(AccumulatedReturn info);

    void rewrite(DirectPhiReturn info);

    void rewrite(AccumulatedPhiReturn info);

    std::optional<ViableReturn> getViableReturn(BasicBlock*) const;

    static bool isCommutativeAndAssociative(ArithmeticInst const* inst);

    Value* identityValue(ArithmeticInst const* inst) const;

    Context& irCtx;
    Function& function;
    utl::small_vector<ViableReturn> returns;
    BasicBlock* loopHeader = nullptr;
    utl::small_vector<Phi*> phiParams;
};

} // namespace

bool opt::tailRecElim(Context& irCtx, Function& function) {
    TREContext ctx(irCtx, function);
    bool const result = ctx.run();
    assertInvariants(irCtx, function);
    return result;
}

bool TREContext::run() {
    gather();
    if (returns.empty()) {
        return false;
    }
    generateLoopHeader();
    for (auto const& ret: returns) {
        utl::visit([this](auto& ret) { rewrite(ret); }, ret);
    }
    for (auto [phi, param]:
         ranges::views::zip(phiParams, function.parameters()))
    {
        replaceValue(&param, phi);
        phi->setArgument(size_t(0), &param);
    }
    return true;
}

void TREContext::gather() {
    /// This gather phase is not smart but rather quick.
    /// It relies on other passes to eliminate dead instructions between the
    /// call and the return.
    for (auto& bb: function) {
        if (auto ret = getViableReturn(&bb)) {
            returns.push_back(*ret);
        }
    }
}

void TREContext::generateLoopHeader() {
    auto* newEntry = new BasicBlock(irCtx, "entry");
    loopHeader     = &function.entry();
    loopHeader->setName("tre.loopheader");
    newEntry->pushBack(new Goto(irCtx, loopHeader));
    function.pushFront(newEntry);

    std::array entryRng = { newEntry };
    auto preds          = ranges::views::concat(entryRng,
                                       returns |
                                           ranges::views::transform(
                                               [](ViableReturn const& ret) {
        return ret.as_base<RetBase>().retInst->parent();
                                           })) |
        ranges::to<utl::small_vector<BasicBlock*, 16>>;
    loopHeader->setPredecessors(preds);
    /// Phis
    std::array entryArg = { PhiMapping{ newEntry, nullptr } };
    auto otherArgs      = returns | ranges::views::transform(
                                   [](ViableReturn const& ret) {
        return PhiMapping{ ret.as_base<RetBase>().retInst->parent(), nullptr };
    });
    auto phiArgs = ranges::views::concat(entryArg, otherArgs) |
                   ranges::to<utl::small_vector<PhiMapping, 8>>;
    for (auto& param: function.parameters() | ranges::views::reverse) {
        phiArgs[0].value = &param;
        auto* phi = new Phi(phiArgs, utl::strcat("tre.param.", param.name()));
        loopHeader->pushFront(phi);
        phiParams.push_back(phi);
    }
}

void TREContext::rewrite(DirectReturn info) {
    auto* bb = info.retInst->parent();
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(bb, arg);
    }
    bb->insert(bb->terminator(), new Goto(irCtx, loopHeader));
    bb->erase(bb->terminator());
}

void TREContext::rewrite(AccumulatedReturn info) {
    auto* bb = info.retInst->parent();
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(bb, arg);
    }
    bb->insert(bb->terminator(), new Goto(irCtx, loopHeader));
    bb->erase(bb->terminator());
    /// Add accumulator to loop header
    auto* accBase =
        new Phi({ { &function.entry(), identityValue(info.accInst) },
                  { bb, info.accInst } },
                "tre.acc");
    loopHeader->insert(loopHeader->phiEnd(), accBase);
    /// Other stuff...
    info.accInst->updateOperand(info.call, accBase);
    bb->erase(info.call);
}

void TREContext::rewrite(DirectPhiReturn info) {
    loopHeader->updatePredecessor(info.retInst->parent(), info.callPred);
    info.retInst->setValue(info.constant);
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(info.callPred, arg);
    }
    info.callPred->insert(info.callPred->terminator(),
                          new Goto(irCtx, loopHeader));
    info.callPred->erase(info.callPred->terminator());
    info.retInst->parent()->removePredecessor(info.callPred);
    info.retInst->parent()->erase(info.phi);
    info.callPred->erase(info.call);
}

void TREContext::rewrite(AccumulatedPhiReturn info) {
    /// Update loop header
    loopHeader->updatePredecessor(info.retInst->parent(), info.accPred);
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(info.accPred, arg);
    }
    /// Update terminators and predecessors
    info.accPred->insert(info.accPred->terminator(),
                         new Goto(irCtx, loopHeader));
    info.accPred->erase(info.accPred->terminator());
    info.retInst->parent()->removePredecessor(info.accPred);
    /// Add accumulator to loop header
    auto* accBase =
        new Phi({ { &function.entry(), identityValue(info.accInst) },
                  { info.accPred, info.accInst } },
                "tre.acc");
    loopHeader->insert(loopHeader->phiEnd(), accBase);
    /// Other stuff...
    info.accInst->updateOperand(info.call, accBase);
    info.accPred->erase(info.call);
    info.retInst->setValue(accBase);
    info.retInst->parent()->erase(info.phi);
}

std::optional<ViableReturn> TREContext::getViableReturn(BasicBlock* bb) const {
    auto* ret = dyncast<Return*>(bb->terminator());
    if (!ret) {
        return std::nullopt;
    }
    // clang-format off
    return visit(*ret->value(), utl::overload{
        [&](Value const& value) -> std::optional<ViableReturn> {
            return std::nullopt;
        },
        [&](Call& call) -> std::optional<ViableReturn> {
            if (call.function() != &function) {
                return std::nullopt;
            }
            return DirectReturn{
              { .retInst = ret },
                .call = &call
            };
        },
        [&](ArithmeticInst& inst) -> std::optional<ViableReturn> {
            if (!isCommutativeAndAssociative(&inst)) {
                return std::nullopt;
            }
            auto [call, other] = std::tuple(inst.lhs(), inst.rhs());
            if (!isa<Call>(call)) {
                std::swap(call, other);
            }
            if (!isa<Call>(call)) {
                return std::nullopt;
            }
            return AccumulatedReturn{
              { .retInst      = ret },
                .accInst      = &inst,
                .call         = cast<Call*>(call),
                .otherAccArg  = other
            };
        },
        [&](Phi& phi) -> std::optional<ViableReturn> {
            if (phi.arguments().size() != 2) {
                return std::nullopt;
            }
            auto get = [&](PhiMapping aArg, PhiMapping bArg) -> std::optional<ViableReturn> {
                auto [aPred, a] = aArg;
                auto [bPred, b] = bArg;
                auto* constant = dyncast<Constant*>(a);
                if (!constant) { return std::nullopt; }
                if (auto* call = dyncast<Call*>(b)) {
                    return DirectPhiReturn{
                      { .retInst      = ret },
                        .phi          = &phi,
                        .constantPred = aPred,
                        .constant     = constant,
                        .callPred     = bPred,
                        .call         = call
                    };
                }
                auto* accOp = dyncast<ArithmeticInst*>(b);
                if (!accOp || !isCommutativeAndAssociative(accOp)) {
                    return std::nullopt;
                }
                auto [call, other] = std::tuple(accOp->lhs(), accOp->rhs());
                if (!isa<Call>(call)) {
                    std::swap(call, other);
                }
                if (!isa<Call>(call)) {
                    return std::nullopt;
                }
                return AccumulatedPhiReturn{
                  { .retInst      = ret },
                    .phi          = &phi,
                    .constantPred = aPred,
                    .constant     = constant,
                    .accPred      = bPred,
                    .accInst      = accOp,
                    .call         = cast<Call*>(call),
                    .otherAccArg  = other
                };
            };
            if (auto res = get(phi.argumentAt(0), phi.argumentAt(1))) {
                return res;
            }
            if (auto res = get(phi.argumentAt(1), phi.argumentAt(0))) {
                return res;
            }
            return std::nullopt;
        }
    }); // clang-format on
}

bool TREContext::isCommutativeAndAssociative(ArithmeticInst const* inst) {
    switch (inst->operation()) {
    case ArithmeticOperation::Add:
    case ArithmeticOperation::Mul:
    case ArithmeticOperation::And:
    case ArithmeticOperation::Or:
//    case ArithmeticOperation::XOr:
        return true;
    default:
        return false;
    }
}

Value* TREContext::identityValue(ArithmeticInst const* inst) const {
    switch (inst->operation()) {
    case ArithmeticOperation::Add:
        return irCtx.integralConstant(APInt(0, 64));
    case ArithmeticOperation::Mul:
        return irCtx.integralConstant(APInt(1, 64));
    case ArithmeticOperation::And:
        return irCtx.integralConstant(APInt(-1, 64));
    case ArithmeticOperation::Or:
        return irCtx.integralConstant(APInt(0, 64));
//    case ArithmeticOperation::XOr:
//        return irCtx.integralConstant(APInt(0, 64));
    default:
        SC_UNREACHABLE();
    }
}
