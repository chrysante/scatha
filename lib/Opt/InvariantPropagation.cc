#include "Opt/Passes.h"

#include <iostream>
#include <queue>
#include <variant>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/Print.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::propagateInvariants, "invprop");

namespace {

/// Represents an invariant of a value at a certain program point
class Invariant {
public:
    Invariant(CompareMode mode, CompareOperation relation, Value* value):
        _mode(mode), _relation(relation), _value(value) {}

    CompareOperation relation() const { return _relation; }

    /// The mode of the comparison
    CompareMode mode() const { return _mode; }

    ///
    Value* value() const { return _value; }

    bool operator==(Invariant const&) const = default;

    size_t hashValue() const {
        return utl::hash_combine(_mode, _relation, _value);
    }

    friend std::ostream& operator<<(std::ostream& str, Invariant const& inv) {
        return str << "(" << inv.relation() << ", " << toString(inv.value())
                   << ")";
    }

private:
    CompareMode _mode;
    CompareOperation _relation;
    Value* _value;
};

} // namespace

template <>
struct std::hash<Invariant> {
    size_t operator()(Invariant const& inv) const { return inv.hashValue(); }
};

namespace {

/// Represent sets of invariants of a values in a basic block
class InvariantSet {
public:
    void insert(Value* value, Invariant inv) { invariants[value].insert(inv); }

    void insert(InvariantSet const& rhs) {
        for (auto&& [value, rhsSet]: rhs.invariants) {
            auto& lhsSet = invariants[value];
            for (auto& rhs: rhsSet) {
                lhsSet.insert(rhs);
            }
        }
    }

    void insert(std::initializer_list<std::pair<Value*, Invariant>> list) {
        for (auto& [value, inv]: list) {
            insert(value, inv);
        }
    }

    /// \Returns the invariants of \p value
    utl::hashset<Invariant> const& operator[](Value* value) {
        return invariants[value];
    }

    bool empty() const { return invariants.empty(); }

    auto const& all() const { return invariants; }

private:
    utl::hashmap<Value*, utl::hashset<Invariant>> invariants;
};

struct IPContext {
    Context& ctx;
    Function& function;
    DominanceInfo const& domInfo;
    DomTree const& domTree;

    utl::hashmap<BasicBlock const*, InvariantSet> invSets;

    IPContext(Context& ctx, Function& function):
        ctx(ctx),
        function(function),
        domInfo(function.getOrComputeDomInfo()),
        domTree(function.getOrComputeDomTree()) {}

    bool run();

    void propagate(BasicBlock* BB);

    void addInvariant(BasicBlock const*, Value* value, Invariant inv);

    bool evaluate(BasicBlock* BB);

    Value* evaluate(Instruction* inst);

    Value* eval(Instruction* inst) { return nullptr; }
    Value* eval(CompareInst* cmp);

    void replaceIfDominatedBy(Value* value,
                              Constant* newValue,
                              BasicBlock const* dom) const;

    Invariant True() const {
        return { CompareMode::Unsigned,
                 CompareOperation::Equal,
                 ctx.integralConstant(true, 1) };
    }

    Invariant False() const {
        return { CompareMode::Unsigned,
                 CompareOperation::Equal,
                 ctx.integralConstant(false, 1) };
    }

    void printInvariants() const;
};

} // namespace

bool opt::propagateInvariants(ir::Context& ctx, ir::Function& function) {
    bool result = IPContext(ctx, function).run();
    return result;
}

bool IPContext::run() {
    bool modified = false;
    std::queue<BasicBlock*> queue;
    utl::hashset<BasicBlock*> visited;
    queue.push(&function.entry());
    visited.insert(&function.entry());
    while (!queue.empty()) {
        auto* BB = queue.front();
        queue.pop();
        modified |= evaluate(BB);
        propagate(BB);
        for (auto* succ: BB->successors()) {
            if (visited.insert(succ).second) {
                queue.push(succ);
            }
        }
    }
    printInvariants();
    return modified;
}

void IPContext::propagate(BasicBlock* BB) {
    /// Propagate invariants originating here
    auto* term = BB->terminator();
    if (auto* branch = dyncast<Branch*>(term)) {
        using enum CompareOperation;
        auto* A = BB->successor(0);
        auto* B = BB->successor(1);
        auto* condition = branch->condition();
        if (A->hasSinglePredecessor()) {
            addInvariant(A, condition, True());
        }
        if (B->hasSinglePredecessor()) {
            addInvariant(B, condition, False());
        }
        if (auto* cmp = dyncast<CompareInst*>(condition)) {
            auto* a = cmp->operandAt(0);
            auto* b = cmp->operandAt(1);
            if (A->hasSinglePredecessor()) {
                addInvariant(A, a, { cmp->mode(), cmp->operation(), b });
                addInvariant(A,
                             b,
                             { cmp->mode(), inverse(cmp->operation()), a });
            }
            if (B->hasSinglePredecessor()) {
                addInvariant(B, b, { cmp->mode(), cmp->operation(), a });
                addInvariant(B,
                             a,
                             { cmp->mode(), inverse(cmp->operation()), b });
            }
        }
    }

    /// Propagate invariants through the nodes dominated by this node
    domTree[BB]->traversePreorder([&](DomTree::Node const* node) {
        auto* sub = node->basicBlock();
        if (BB == sub) {
            return;
        }
        invSets[sub].insert(invSets[BB]);
    });
}

void IPContext::addInvariant(BasicBlock const* BB,
                             Value* value,
                             Invariant inv) {
    if (isa<Constant>(value)) {
        return;
    }
    if (value == inv.value()) {
        return;
    }
    if (auto* constant = dyncast<Constant*>(inv.value());
        inv.relation() == CompareOperation::Equal && constant)
    {
        replaceIfDominatedBy(value, constant, BB);
        return;
    }
    invSets[BB].insert(value, inv);
}

bool IPContext::evaluate(BasicBlock* BB) {
    bool result = false;
    for (auto& inst: *BB) {
        if (auto* newVal = evaluate(&inst)) {
            replaceValue(&inst, newVal);
            result = true;
        }
    }
    return result;
}

Value* IPContext::evaluate(Instruction* inst) {
    return visit(*inst, [this](auto& inst) { return eval(&inst); });
}

Value* IPContext::eval(CompareInst* cmp) { return nullptr; }

void IPContext::replaceIfDominatedBy(Value* value,
                                     Constant* newValue,
                                     BasicBlock const* dom) const {
    auto users = value->users() | Filter<Instruction> |
                 ranges::to<utl::small_vector<Instruction*>>;
    for (auto* user: users) {
        if (domInfo.dominatorSet(user->parent()).contains(dom)) {
            user->updateOperand(value, newValue);
        }
    }
}

void IPContext::printInvariants() const {
    using enum Level;
    TreeFormatter formatter;
    for (auto& BB: function) {
        auto setItr = invSets.find(&BB);
        if (setItr == invSets.end()) {
            continue;
        }
        formatter.push(&BB != &function.back() ? Child : LastChild);
        auto& set = setItr->second;
        std::cout << formatter.beginLine() << BB.name() << ":" << std::endl;
        for (auto&& [index, elem]: set.all() | ranges::views::enumerate) {
            auto&& [value, invs] = elem;
            formatter.push(index != set.all().size() - 1 ? Child : LastChild);
            std::cout << formatter.beginLine() << "%" << value->name()
                      << std::endl;
            for (auto&& [index, inv]: invs | ranges::views::enumerate) {
                formatter.push(index != invs.size() - 1 ? Child : LastChild);
                std::cout << formatter.beginLine() << inv << std::endl;
                formatter.pop();
            }
            formatter.pop();
        }
        formatter.pop();
    }
}
