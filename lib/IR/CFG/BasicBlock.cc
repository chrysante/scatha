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

void BasicBlock::insertPhi(Phi* phiNode) {
    auto itr = begin();
    while (isa<Phi>(*itr)) {
        ++itr;
    }
    insert(itr, phiNode);
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

bool BasicBlock::hasPhiNodes() const { return isa<Phi>(front()); }

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
    auto itr = ranges::find(preds, pred);
    auto index = utl::narrow_cast<size_t>(itr - ranges::begin(preds));
    removePredecessor(index);
}

void BasicBlock::removePredecessor(size_t index) {
    SC_ASSERT(index < preds.size(), "Invalid index");
    auto itr = preds.begin() + index;
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

void BasicBlock::eraseCallback(Instruction const& cinst) {
    /// The assertion for `inst.users().empty()` is omitted.
    /// While on the surface that check may look natural, when erasing multiple
    /// instructions at the same time, that reference each other, this check
    /// will fail if we don't clean up all the references before. But cleaning
    /// the references may be unnecessary work as the instructions are deleted
    /// anyway.
    auto& inst = const_cast<Instruction&>(cinst);
    inst.clearOperands();
    parent()->nameFac.erase(inst.name());
}

bool BasicBlock::isEntry() const {
    return parent()->begin().to_address() == this;
}
