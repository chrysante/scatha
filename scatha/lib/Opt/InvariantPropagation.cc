#include "Opt/Passes.h"

#include <iostream>
#include <queue>
#include <span>
#include <variant>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"
#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Dominance.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

#if 0
SC_REGISTER_PASS(opt::propagateInvariants, "invprop");
#endif

[[maybe_unused]] static constexpr auto Signed = CompareMode::Signed;
[[maybe_unused]] static constexpr auto Unsigned = CompareMode::Unsigned;
[[maybe_unused]] static constexpr auto Float = CompareMode::Float;

[[maybe_unused]] static constexpr auto Less = CompareOperation::Less;
[[maybe_unused]] static constexpr auto LessEq = CompareOperation::LessEq;
[[maybe_unused]] static constexpr auto Greater = CompareOperation::Greater;
[[maybe_unused]] static constexpr auto GreaterEq = CompareOperation::GreaterEq;
[[maybe_unused]] static constexpr auto Equal = CompareOperation::Equal;
[[maybe_unused]] static constexpr auto NotEqual = CompareOperation::NotEqual;

namespace {

///
class InvariantKey {
public:
    InvariantKey(CompareMode mode, CompareOperation relation):
        _mode(mode), _relation(relation) {}

    CompareOperation relation() const { return _relation; }

    /// The mode of the comparison
    CompareMode mode() const { return _mode; }

    bool operator==(InvariantKey const&) const = default;

    size_t hashValue() const { return utl::hash_combine(_mode, _relation); }

private:
    CompareMode _mode;
    CompareOperation _relation;
};

} // namespace

template <>
struct std::hash<InvariantKey> {
    size_t operator()(InvariantKey const& inv) const { return inv.hashValue(); }
};

[[maybe_unused]] static bool compareSigned(CompareOperation op,
                                           APInt const& lhs, APInt const& rhs) {
    switch (op) {
    case Less:
        return scmp(lhs, rhs) < 0;
    case LessEq:
        return scmp(lhs, rhs) <= 0;
    case Greater:
        return scmp(lhs, rhs) > 0;
    case GreaterEq:
        return scmp(lhs, rhs) >= 0;
    case Equal:
        return scmp(lhs, rhs) == 0;
    case NotEqual:
        return scmp(lhs, rhs) != 0;
    default:
        SC_UNREACHABLE();
    }
}

[[maybe_unused]] static bool compareUnsigned(CompareOperation op,
                                             APInt const& lhs,
                                             APInt const& rhs) {
    switch (op) {
    case Less:
        return ucmp(lhs, rhs) < 0;
    case LessEq:
        return ucmp(lhs, rhs) <= 0;
    case Greater:
        return ucmp(lhs, rhs) > 0;
    case GreaterEq:
        return ucmp(lhs, rhs) >= 0;
    case Equal:
        return ucmp(lhs, rhs) == 0;
    case NotEqual:
        return ucmp(lhs, rhs) != 0;
    default:
        SC_UNREACHABLE();
    }
}

[[maybe_unused]] static bool compareFloat(CompareOperation op,
                                          APFloat const& lhs,
                                          APFloat const& rhs) {
    switch (op) {
    case Less:
        return cmp(lhs, rhs) < 0;
    case LessEq:
        return cmp(lhs, rhs) <= 0;
    case Greater:
        return cmp(lhs, rhs) > 0;
    case GreaterEq:
        return cmp(lhs, rhs) >= 0;
    case Equal:
        return cmp(lhs, rhs) == 0;
    case NotEqual:
        return cmp(lhs, rhs) != 0;
    default:
        SC_UNREACHABLE();
    }
}

[[maybe_unused]] static bool compare(CompareMode mode, CompareOperation op,
                                     Constant const* lhs, Constant const* rhs) {
    switch (mode) {
    case Signed:
        return compareSigned(op, cast<IntegralConstant const*>(lhs)->value(),
                             cast<IntegralConstant const*>(rhs)->value());
    case Unsigned:
        return compareUnsigned(op, cast<IntegralConstant const*>(lhs)->value(),
                               cast<IntegralConstant const*>(rhs)->value());
    case Float:
        return compareFloat(op,
                            cast<FloatingPointConstant const*>(lhs)->value(),
                            cast<FloatingPointConstant const*>(rhs)->value());
    default:
        SC_UNREACHABLE();
    }
}

namespace {

/// Represents a set of invariants with a specifiy relational key of a value at
/// a certain program point
class Invariant {
public:
    Invariant(InvariantKey key): _key(key) {}

    InvariantKey key() const { return _key; }

    std::span<Value* const> values() const { return _values; }

    void insert([[maybe_unused]] Value* value) {
#if 0
        if (_values.empty()) {
            _values.push_back(value);
            return;
        }
        if (ranges::contains(_values, value)) {
            return;
        }
        switch (key().relation()) {
        case Less: [[fallthrough]];
        case LessEq: [[fallthrough]];
        case Greater: [[fallthrough]];
        case GreaterEq:
            if (auto* constant = dyncast<Constant*>(value)) {
                if (auto* existing = dyncast<Constant*>(_values.front())) {
                    if (compare(key().mode(), key.relation(), constant, existing)) {
                        
                    }
                    else {
                        
                    }
                }
                else {
                    
                }
            }
            else {
                push_bac
            }
        case Equal: [[fallthrough]];
        case NotEqual:
            _values.push_back(value);
        default: SC_UNREACHABLE();
        }
#endif
    }

private:
    InvariantKey _key;
    utl::small_vector<Value*> _values;
};

/// Represent sets of invariants of a values in a basic block
class InvariantSet {
public:
    void insert(Value* value, InvariantKey key, Value* rhs) {
        invariants[value].insert({ key, rhs });
    }

    void insert(InvariantSet const& rhs) {
        for (auto&& [value, rhsSet]: rhs.invariants) {
            auto& lhsSet = invariants[value];
            for (auto& [key, rhs]: rhsSet) {
                lhsSet.insert({ key, rhs });
            }
        }
    }

    void insert(
        std::initializer_list<std::tuple<Value*, InvariantKey, Value*>> list) {
        for (auto& [value, key, rhs]: list) {
            insert(value, key, rhs);
        }
    }

    /// \Returns the invariants of \p value
    utl::hashmap<InvariantKey, Value*> const& operator[](Value* value) {
        return invariants[value];
    }

    bool empty() const { return invariants.empty(); }

    auto const& all() const { return invariants; }

private:
    utl::hashmap<Value*, utl::hashmap<InvariantKey, Value*>> invariants;
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

    void addInvariant(BasicBlock const*, Value* value, InvariantKey key,
                      Value*);

    bool evaluate(BasicBlock* BB);

    Value* evaluate(Instruction* inst);

    Value* eval(Instruction*) { return nullptr; }
    Value* eval(CompareInst* cmp);

    void replaceIfDominatedBy(Value* value, Constant* newValue,
                              BasicBlock const* dom) const;

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
            addInvariant(A, condition, { Unsigned, Equal },
                         ctx.intConstant(true, 1));
        }
        if (B->hasSinglePredecessor()) {
            addInvariant(B, condition, { Unsigned, Equal },
                         ctx.intConstant(false, 1));
        }
        if (auto* cmp = dyncast<CompareInst*>(condition)) {
            auto* a = cmp->operandAt(0);
            auto* b = cmp->operandAt(1);
            if (A->hasSinglePredecessor()) {
                addInvariant(A, a, { cmp->mode(), cmp->operation() }, b);
                addInvariant(A, b, { cmp->mode(), inverse(cmp->operation()) },
                             a);
            }
            if (B->hasSinglePredecessor()) {
                addInvariant(B, b, { cmp->mode(), cmp->operation() }, a);
                addInvariant(B, a, { cmp->mode(), inverse(cmp->operation()) },
                             b);
            }
        }
    }

    /// Propagate invariants through the nodes dominated by this node
    domTree[BB]->preorderDFS([&](DomTree::Node const* node) {
        auto* sub = node->basicBlock();
        if (BB == sub) {
            return;
        }
        invSets[sub].insert(invSets[BB]);
    });
}

void IPContext::addInvariant(BasicBlock const* BB, Value* value,
                             InvariantKey key, Value* rhs) {
    if (isa<Constant>(value)) {
        return;
    }
    if (value == rhs) {
        return;
    }
    if (auto* constant = dyncast<Constant*>(rhs);
        key.relation() == CompareOperation::Equal && constant)
    {
        replaceIfDominatedBy(value, constant, BB);
        return;
    }
    invSets[BB].insert(value, key, rhs);
}

bool IPContext::evaluate(BasicBlock* BB) {
    bool result = false;
    for (auto& inst: *BB) {
        if (auto* newVal = evaluate(&inst)) {
            inst.replaceAllUsesWith(newVal);
            result = true;
        }
    }
    return result;
}

Value* IPContext::evaluate(Instruction* inst) {
    return visit(*inst, [this](auto& inst) { return eval(&inst); });
}

Value* IPContext::eval(CompareInst*) { return nullptr; }

void IPContext::replaceIfDominatedBy(Value* value, Constant* newValue,
                                     BasicBlock const* dom) const {
    auto users = value->users() | Filter<Instruction> | ToSmallVector<>;
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
                auto [key, rhs] = inv;
                formatter.push(index != invs.size() - 1 ? Child : LastChild);
                std::cout << formatter.beginLine() << key.relation() << " "
                          << toString(rhs) << std::endl;
                formatter.pop();
            }
            formatter.pop();
        }
        formatter.pop();
    }
}
