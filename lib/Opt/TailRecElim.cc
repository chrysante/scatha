#include "Opt/TailRecElim.h"

#include <utl/variant.hpp>
#include <utl/vector.hpp>

#include "IR/Context.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct RetBase {
    Return* retInst;
    
};

struct DirectReturn: RetBase {
    
};

struct DirectPhiReturn: RetBase {
    BasicBlock* constantPred;
    Value* constant;
    BasicBlock* callPred;
    Call* call;
};

struct AccumulatedPhiReturn: RetBase {
    Value* constant;
    ArithmeticInst* accInst;
    Call* call;
    Value* otherAccArg;
};

using ViableReturn = utl::variant<DirectReturn, DirectPhiReturn, AccumulatedPhiReturn>;

struct TREContext {
    TREContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}
    
    bool run();

    void gather();
    
    void generateLoopHeader();
    
    void rewrite(DirectReturn info);
    
    void rewrite(DirectPhiReturn info);
    
    void rewrite(AccumulatedPhiReturn info);
    
    static std::optional<ViableReturn> getViableReturn(BasicBlock*);
    
    static bool isCommutativeAndAssociative(ArithmeticInst const* inst);
    
    Context& irCtx;
    Function& function;
    utl::small_vector<ViableReturn> returns;
    BasicBlock* loopHeader = nullptr;
    utl::small_vector<Phi*> phiParams;
};

} // namespace

bool opt::tailRecElim(Context& irCtx, Function& function) {
    TREContext ctx(irCtx, function);
    return ctx.run();
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
    loopHeader = new BasicBlock(irCtx, "tre.loopheader");
    BasicBlock* entry = &function.entry();
    
    auto entryTerm = entry->extract(entry->terminator());
    loopHeader->pushBack(entryTerm.release());
    for (auto* succ: loopHeader->successors()) {
        succ->updatePredecessor(entry, loopHeader);
    }
    
    entry->pushBack(new Goto(irCtx, loopHeader));
    std::array entryRng = { entry };
    auto  preds =
        ranges::views::concat(entryRng,
                              returns |
                              ranges::views::transform([](ViableReturn const& ret) {
                                  return ret.as_base<RetBase>().retInst->parent();
                              })) |
        ranges::to<utl::small_vector<BasicBlock*, 16>>;
    loopHeader->setPredecessors(preds);
    function.insert(entry->next(), loopHeader);
    
    /// Phis
    std::array entryArg = { PhiMapping{ entry, nullptr } };
    auto phiArgs =
        ranges::views::concat(entryArg,
                              returns |
                              ranges::views::transform([](ViableReturn const& ret) {
                                  return PhiMapping{
                                      ret.as_base<RetBase>().retInst->parent(),
                                      nullptr
                                  };
                              })) |
        ranges::to<utl::small_vector<PhiMapping, 8>>;
    
    for (auto& param: function.parameters()) {
        phiArgs[0].value = &param;
        auto* phi = new Phi(phiArgs, utl::strcat("tre.param.", param.name()));
        loopHeader->insert(loopHeader->terminator(), phi);
        phiParams.push_back(phi);
    }
}

void TREContext::rewrite(DirectReturn info) {
    
}

void TREContext::rewrite(DirectPhiReturn info) {
    loopHeader->updatePredecessor(info.retInst->parent(), info.callPred);
    info.retInst->setValue(info.constant);
    for (auto [phi, arg]: ranges::views::zip(phiParams, info.call->arguments())) {
        phi->setArgument(info.callPred, arg);
    }
    info.callPred->insert(info.callPred->terminator(), new Goto(irCtx, loopHeader));
    info.callPred->erase(info.callPred->terminator());
    info.retInst->parent()->removePredecessor(info.callPred);
    info.callPred->erase(info.call);
}

void TREContext::rewrite(AccumulatedPhiReturn info) {
    
}

std::optional<ViableReturn> TREContext::getViableReturn(BasicBlock* bb) {
    auto* ret = dyncast<Return*>(bb->terminator());
    if (!ret) { return std::nullopt; }
    // clang-format off
    return visit(*ret->value(), utl::overload{
        [&](Value& value) -> std::optional<ViableReturn> {
            return std::nullopt;
        },
        [&](Call& call) -> std::optional<ViableReturn> { return DirectReturn{ ret }; },
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
                        .constantPred = aPred,
                        .constant     = constant,
                        .callPred     = bPred,
                        .call         = call
                    };
                }
                if (auto* accOp = dyncast<ArithmeticInst*>(b)) {
                    auto [call, other] = std::tuple(accOp->lhs(), accOp->rhs());
                    if (!isa<Call>(call)) {
                        std::swap(call, other);
                    }
                    if (!isa<Call>(call)) {
                        return std::nullopt;
                    }
                    return AccumulatedPhiReturn{
                        { .retInst     = ret },
                        .constant    = constant,
                        .accInst     = accOp,
                        .call        = cast<Call*>(call),
                        .otherAccArg = other
                    };
                }
                return std::nullopt;
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
        return true;
    default:
        return false;
    }
}
