#include "CodeGen/CodeGen2/InterferenceGraph.h"

#include "IR/CFG.h"
#include "IR/DataFlow.h" // For live sets
#include "IR/Type.h"

using namespace scatha;
using namespace cg;
using namespace ir;

InterferenceGraph InterferenceGraph::compute(Function const& function) {
    InterferenceGraph result;
    result.computeImpl(function);
    return result;
}

void InterferenceGraph::colorize(size_t maxColors) {
    numCols = maxColors;
    for (auto& n: nodes) {
        n->col = 0;
    }
}

void InterferenceGraph::computeImpl(Function const& function) {
    for (auto& param: function.parameters()) {
        addValue(&param);
    }
    for (auto& inst: function.instructions()) {
        if (auto* convInst = dyncast<ConversionInst const*>(&inst)) {
            auto const conv = convInst->conversion();
            if (conv == Conversion::Zext || conv == Conversion::Trunc ||
                conv == Conversion::Bitcast)
            {
                /// These are essentially no-ops, and thus can share a register
                /// with their operand.
                auto* node = find(convInst->operand());
                node->vals.push_back(convInst);
                valueMap.insert({ convInst, node });
                continue;
            }
        }
        if (auto* gep = dyncast<GetElementPointer const*>(&inst)) {
            bool const allUsersAreLoadsAndStores =
                ranges::all_of(gep->users(), [](ir::User const* user) {
                    return isa<Load>(user) || isa<Store>(user);
                });
            if (allUsersAreLoadsAndStores) {
                continue;
            }
        }
        if (!isa<VoidType>(inst.type())) {
            addValue(&inst);
        }
    }
    auto liveSets = LiveSets::compute(function);
    for (auto& param: function.parameters()) {
        addEdges(&param, liveSets.live(&function.entry()).liveIn);
    }
    for (auto& [BB, liveSetsOfBB]: liveSets) {
        auto live          = liveSetsOfBB.liveOut;
        auto const& liveIn = liveSetsOfBB.liveIn;
        for (auto& inst: *BB | ranges::views::reverse) {
            if (!isa<VoidType>(inst.type())) {
                addEdges(&inst, live);
            }
            for (auto* op: inst.operands()) {
                if (liveIn.contains(op)) {
                    live.insert(op);
                }
            }
            live.erase(&inst);
        }
    }
}

void InterferenceGraph::addValue(Value const* value) {
    auto nodeOwner = std::make_unique<Node>(value);
    auto* node     = nodeOwner.get();
    nodes.push_back(std::move(nodeOwner));
    valueMap.insert({ value, node });
}

void InterferenceGraph::addEdges(ir::Value const* value, auto&& listOfValues) {
    Node* valNode = find(value);
    for (auto* rhs: listOfValues) {
        Node* rhsNode = find(rhs);
        if (valNode == rhsNode) {
            continue;
        }
        valNode->addNeighbour(rhsNode);
        rhsNode->addNeighbour(valNode);
    }
}

InterferenceGraph::Node* InterferenceGraph::find(ir::Value const* value) {
    auto itr = valueMap.find(value);
    SC_ASSERT(itr != valueMap.end(), "Not found");
    return itr->second;
}
