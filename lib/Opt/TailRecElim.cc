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
    Value* constant;
    Return* otherReturn;
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

    void rewrite(ViableReturn const& ret) {
        generateLoopHeader();
        utl::visit([this](auto& ret) { rewriteImpl(ret); }, ret);
    }

    void rewriteImpl(DirectReturn info);

    void rewriteImpl(AccumulatedReturn info);

    void rewriteImpl(DirectPhiReturn info);

    void rewriteImpl(AccumulatedPhiReturn info);

    std::optional<ViableReturn> getViableReturn(Return& ret) const;

    bool isInterestingCall(Value const* inst) const;

    static bool isCommutativeAndAssociative(ArithmeticInst const* inst);

    Value* identityValue(ArithmeticInst const* inst) const;

    Context& irCtx;
    Function& function;
    utl::small_vector<ViableReturn> viableReturns;
    utl::small_vector<Return*> otherReturns;
    size_t totalReturns    = 0;
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
    if (viableReturns.empty()) {
        return false;
    }
    switch (totalReturns) {
    case 1: {
        auto ret = viableReturns.front();
        rewrite(ret);
        break;
    }
    case 2: {
        if (viableReturns.size() > 1) {
            return false;
        }
        auto ret   = viableReturns.front();
        auto other = otherReturns.front();
        // clang-format off
        bool const modified = visit(utl::overload{
            [&](DirectReturn const& ret) {
                rewrite(ret);
                return true;
            },
            [&](AccumulatedReturn ret) {
                auto* otherConstRetval = dyncast<Constant*>(other->value());
                if (!otherConstRetval) {
                    return false;
                }
                ret.constant = otherConstRetval;
                ret.otherReturn = other;
                rewrite(ret);
                return true;
            },
            [&](DirectPhiReturn const&) { return false; },
            [&](AccumulatedPhiReturn const&) { return false; },
        }, ret); // clang-format on
        if (!modified) {
            return false;
        }
        break;
    }
    default:
        return false;
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
        auto* ret = dyncast<Return*>(bb.terminator());
        if (!ret) {
            continue;
        }
        ++totalReturns;
        if (auto viableRet = getViableReturn(*ret)) {
            viableReturns.push_back(*viableRet);
        }
        else {
            otherReturns.push_back(ret);
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
                                       viableReturns |
                                           ranges::views::transform(
                                               [](ViableReturn const& ret) {
        return ret.as_base<RetBase>().retInst->parent();
                                           })) |
        ranges::to<utl::small_vector<BasicBlock*, 16>>;
    loopHeader->setPredecessors(preds);
    /// ## Phis for parameters
    /// For every function parameter, we add a phi node to the loopheader block.
    /// The first argument to the phi function will be the actual parameter.
    /// The next arguments will be the corresponding argument of the recursive
    /// calls.
    std::array entryArg = { PhiMapping{ newEntry, nullptr } };
    auto otherArgs      = viableReturns | ranges::views::transform(
                                         [](ViableReturn const& ret) {
        return PhiMapping{ ret.as_base<RetBase>().retInst->parent(), nullptr };
    });
    auto phiArgs = ranges::views::concat(entryArg, otherArgs) |
                   ranges::to<utl::small_vector<PhiMapping, 8>>;
    auto before = loopHeader->phiEnd();
    for (auto& param: function.parameters()) {
        phiArgs[0].value = &param;
        auto* phi = new Phi(phiArgs, utl::strcat("tre.param.", param.name()));
        loopHeader->insert(before, phi);
        phiParams.push_back(phi);
    }
}

void TREContext::rewriteImpl(DirectReturn info) {
    auto* bb = info.retInst->parent();
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(bb, arg);
    }
    bb->insert(bb->terminator(), new Goto(irCtx, loopHeader));
    bb->erase(bb->terminator());
    bb->erase(info.call);
}

void TREContext::rewriteImpl(AccumulatedReturn info) {
    auto* bb = info.retInst->parent();
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments()))
    {
        phi->setArgument(bb, arg);
    }
    bb->insert(bb->terminator(), new Goto(irCtx, loopHeader));
    bb->erase(bb->terminator());
    /// Add accumulator to loop header
    auto* startValue =
        info.constant ? info.constant : identityValue(info.accInst);
    auto* acc =
        new Phi({ { &function.entry(), startValue }, { bb, info.accInst } },
                "tre.acc");
    loopHeader->insert(loopHeader->phiEnd(), acc);
    /// Other stuff...
    info.accInst->updateOperand(info.call, acc);
    bb->erase(info.call);
    if (info.otherReturn) {
        info.otherReturn->setValue(acc);
    }
}

void TREContext::rewriteImpl(DirectPhiReturn info) {
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

void TREContext::rewriteImpl(AccumulatedPhiReturn info) {
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
    auto* acc = new Phi({ { &function.entry(), info.constant },
                          { info.accPred, info.accInst } },
                        "tre.acc");
    loopHeader->insert(loopHeader->phiEnd(), acc);
    /// Other stuff...
    info.accInst->updateOperand(info.call, acc);
    info.accPred->erase(info.call);
    info.retInst->setValue(acc);
    info.retInst->parent()->erase(info.phi);
}

std::optional<ViableReturn> TREContext::getViableReturn(Return& ret) const {
    // clang-format off
    return visit(*ret.value(), utl::overload{
        [&](Value const& value) -> std::optional<ViableReturn> {
            return std::nullopt;
        },
        [&](Call& call) -> std::optional<ViableReturn> {
            if (call.function() != &function) {
                return std::nullopt;
            }
            return DirectReturn{
              { .retInst = &ret },
                .call = &call
            };
        },
        [&](ArithmeticInst& inst) -> std::optional<ViableReturn> {
            if (!isCommutativeAndAssociative(&inst)) {
                return std::nullopt;
            }
            auto [call, other] = std::tuple(inst.lhs(), inst.rhs());
            if (!isInterestingCall(call)) {
                std::swap(call, other);
                if (!isInterestingCall(call)) {
                    return std::nullopt;
                }
            }
            return AccumulatedReturn{
              { .retInst      = &ret },
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
                    if (call->function() != &function) {
                        return std::nullopt;
                    }
                    return DirectPhiReturn{
                      { .retInst      = &ret },
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
                if (!isInterestingCall(call)) {
                    std::swap(call, other);
                    if (!isInterestingCall(call)) {
                        return std::nullopt;
                    }
                }
                return AccumulatedPhiReturn{
                  { .retInst      = &ret },
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

bool TREContext::isInterestingCall(Value const* inst) const {
    auto* call = dyncast<Call const*>(inst);
    return call && call->function() == &function;
}

bool TREContext::isCommutativeAndAssociative(ArithmeticInst const* inst) {
    switch (inst->operation()) {
    case ArithmeticOperation::Add:
    case ArithmeticOperation::Mul:
    case ArithmeticOperation::And:
    case ArithmeticOperation::Or:
    case ArithmeticOperation::XOr:
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
