#include "IR/CFG/BasicBlock.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

BasicBlock::BasicBlock(Context& context, std::string name):
    Value(NodeType::BasicBlock, context.voidType(), std::move(name)) {}

void BasicBlock::eraseAllPhiNodes() {
    auto phiEnd = begin();
    while (isa<Phi>(*phiEnd)) {
        ++phiEnd;
    }
    erase(begin(), phiEnd);
}

bool BasicBlock::contains(Instruction const& inst) const {
    auto const itr = std::find_if(begin(), end(), [&](Instruction const& i) {
        return &i == &inst;
    });
    return itr != end();
}

bool BasicBlock::emptyExceptTerminator() const {
    return terminator() == &front();
}

void BasicBlock::updatePredecessor(BasicBlock const* oldPred,
                                   BasicBlock* newPred) {
    auto itr = ranges::find(preds, oldPred);
    SC_ASSERT(itr != ranges::end(preds), "Not found");
    *itr = newPred;
    for (auto& phi: phiNodes()) {
        size_t const index = phi.indexOf(oldPred);
        phi.setPredecessor(index, newPred);
    }
}

void BasicBlock::removePredecessor(BasicBlock const* pred) {
    auto itr   = ranges::find(preds, pred);
    auto index = utl::narrow_cast<size_t>(itr - ranges::begin(preds));
    removePredecessor(index);
}

void BasicBlock::removePredecessor(size_t index) {
    SC_ASSERT(index < preds.size(), "Invalid index");
    auto itr   = preds.begin() + index;
    auto* pred = *itr;
    preds.erase(itr);
    for (auto& phi: phiNodes()) {
        phi.removeArgument(pred);
    }
}

void BasicBlock::insertCallback(Instruction& inst) {
    inst.set_parent(this);
    if (auto* func = parent()) {
        inst.uniqueExistingName(*func);
    }
}

void BasicBlock::eraseCallback(Instruction const& inst) {
    const_cast<Instruction&>(inst).clearOperands();
    parent()->nameFac.erase(inst.name());
}

bool BasicBlock::isEntry() const {
    return parent()->begin().to_address() == this;
}

static auto phiEndImpl(auto begin, auto end) {
    while (begin != end && isa<Phi>(begin.to_address())) {
        ++begin;
    }
    return begin;
}

BasicBlock::Iterator BasicBlock::phiEnd() { return phiEndImpl(begin(), end()); }

BasicBlock::ConstIterator BasicBlock::phiEnd() const {
    return phiEndImpl(begin(), end());
}
