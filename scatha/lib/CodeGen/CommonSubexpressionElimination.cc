#include "CodeGen/Passes.h"

#include <range/v3/numeric.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "CodeGen/Utility.h"
#include "MIR/CFG.h"
#include "MIR/Hash.h"
#include "MIR/Instructions.h"

using namespace scatha;
using namespace cg;
using namespace mir;
using namespace ranges::views;

static decltype(auto) visitInstPair(Instruction const& A, Instruction const& B,
                                    auto&& visitor) {
    SC_EXPECT(A.instType() == B.instType());
    return visit(A, [&]<typename Derived>(Derived const& A) -> decltype(auto) {
        return std::invoke(visitor, A, cast<Derived const&>(B));
    });
}

namespace {

class Expression {
public:
    explicit Expression(Instruction& inst): _inst(&inst) {}

    bool operator==(Expression rhs) const {
        if (inst().instType() != rhs.inst().instType()) {
            return false;
        }
#define TEST_EQ(property)       A.property() == B.property()
#define TEST_RANGE_EQ(property) ranges::equal(A.property(), B.property())
        // clang-format off
        bool result = visitInstPair(inst(), rhs.inst(), csp::overload{
            [](Instruction const& A, Instruction const& B) {
                return TEST_RANGE_EQ(operands) && TEST_EQ(bytewidth);
            },
            [](StoreInst const& A, StoreInst const& B) {
                return TEST_EQ(address) && TEST_EQ(source) &&
                       TEST_EQ(bytewidth);
            },
            [](LoadInst const& A, LoadInst const& B) {
                return TEST_EQ(address) && TEST_EQ(bytewidth);
            },
            [](CondCopyInst const& A, CondCopyInst const& B) {
                return TEST_EQ(source) && TEST_EQ(condition) &&
                       TEST_EQ(bytewidth);
            },
            [](LEAInst const& A, LEAInst const& B) {
                return TEST_EQ(address) && TEST_EQ(bytewidth);
            },
            [](CompareInst const& A, CompareInst const& B) {
                return TEST_EQ(LHS) && TEST_EQ(RHS) && TEST_EQ(mode) &&
                       TEST_EQ(bytewidth);
            },
            [](TestInst const& A, TestInst const& B) {
                return TEST_EQ(operand) && TEST_EQ(mode) && TEST_EQ(bytewidth);
            },
            [](SetInst const& A, SetInst const& B) {
                return TEST_EQ(operation) && TEST_EQ(bytewidth);
            },
            [](UnaryArithmeticInst const& A, UnaryArithmeticInst const& B) {
                return TEST_EQ(operand) && TEST_EQ(operation) &&
                       TEST_EQ(bytewidth);
            },
            [](ValueArithmeticInst const& A, ValueArithmeticInst const& B) {
                return TEST_EQ(LHS) && TEST_EQ(RHS) &&
                       TEST_EQ(operation) && TEST_EQ(bytewidth);
            },
            [](LoadArithmeticInst const& A, LoadArithmeticInst const& B) {
                return TEST_EQ(LHS) && TEST_EQ(RHS) &&
                       TEST_EQ(operation) && TEST_EQ(bytewidth);
            },
            [](ConversionInst const& A, ConversionInst const& B) {
                return TEST_EQ(operand) && TEST_EQ(conversion) &&
                       TEST_EQ(fromBits) && TEST_EQ(toBits) &&
                       TEST_EQ(bytewidth);
            },
            [](CondJumpInst const& A, CondJumpInst const& B) {
                return TEST_EQ(target) && TEST_EQ(condition) &&
                       TEST_EQ(bytewidth);
            },
        }); // clang-format on
#undef TEST_EQ
#undef TEST_RANGE_EQ
        return result;
    }

    size_t hash() const {
        // clang-format off
        return SC_MATCH(inst()) {
            [](Instruction const& inst) {
                return utl::hash_combine_range(inst.operands().begin(),
                                               inst.operands().end());
            },
            [](StoreInst const& inst) {
                return utl::hash_combine(inst.address(), inst.source());
            },
            [](LoadInst const& inst) {
                return utl::hash_combine(inst.address());
            },
            [](CondCopyInst const& inst) {
                return utl::hash_combine(inst.source(), inst.condition());
            },
            [](LEAInst const& inst) {
                return utl::hash_combine(inst.address());
            },
            [](CompareInst const& inst) {
                return utl::hash_combine(inst.LHS(), inst.RHS(), inst.mode());
            },
            [](TestInst const& inst) {
                return utl::hash_combine(inst.operand(), inst.mode());
            },
            [](SetInst const& inst) {
                return utl::hash_combine(inst.operation());
            },
            [](UnaryArithmeticInst const& inst) {
                return utl::hash_combine(inst.operand(), inst.operation());
            },
            [](ValueArithmeticInst const& inst) {
                return utl::hash_combine(inst.LHS(), inst.RHS(), inst.operation());
            },
            [](LoadArithmeticInst const& inst) {
                return utl::hash_combine(inst.LHS(), inst.RHS(), inst.operation());
            },
            [](ConversionInst const& inst) {
                return utl::hash_combine(inst.operand(), inst.conversion(),
                                         inst.fromBits(), inst.toBits());
            },
            [](CondJumpInst const& inst) {
                return utl::hash_combine(inst.target(), inst.condition());
            },
        }; // clang-format on
    }

    Instruction& inst() const { return *_inst; }

private:
    Instruction* _inst = nullptr;
};

} // namespace

template <>
struct std::hash<Expression> {
    size_t operator()(Expression expr) const { return expr.hash(); }
};

namespace {

struct CSEContext {
    mir::Context& ctx;
    mir::Function& F;
    mir::BasicBlock& BB;

    CSEContext(mir::Context& ctx, mir::Function& F, mir::BasicBlock& BB):
        ctx(ctx), F(F), BB(BB) {}

    bool run();

    utl::small_vector<utl::hashset<Instruction*>> computeRankMap();
};

} // namespace

bool cg::commonSubexpressionElimination(mir::Context& ctx, mir::Function& F) {
    bool modified = false;
    for (auto& BB: F) {
        modified |= CSEContext(ctx, F, BB).run();
    }
    return modified;
}

static bool hasLocalSideeffects(Instruction const& inst) {
    return isa<SetInst>(inst) || isa<TestInst>(inst) || hasSideEffects(inst);
}

bool CSEContext::run() {
    auto rankMap = computeRankMap();
    utl::small_vector<Instruction*> toErase;
    for (auto& rank: rankMap) {
        utl::hashset<Expression> table;
        for (auto* inst: rank) {
            /// We don't eliminate loads and instructions with side effects
            if (isa<LoadInst>(inst) || hasLocalSideeffects(*inst)) {
                continue;
            }
            auto [itr, success] = table.insert(Expression(*inst));
            /// If we don't have this expression already in the block we
            /// continue
            if (success) {
                continue;
            }
            auto* existing = &itr->inst();
            for (auto [dest, repl]:
                 zip(inst->destRegisters(), existing->destRegisters()))
            {
                dest->replaceUsesWith(repl);
            }
            toErase.push_back(inst);
        }
    }
    for (auto* inst: toErase) {
        BB.erase(inst);
    }
    return !toErase.empty();
}

utl::small_vector<utl::hashset<Instruction*>> CSEContext::computeRankMap() {
    utl::hashmap<Value const*, size_t> ranks;
    utl::small_vector<utl::hashset<Instruction*>> result;
    auto get = [&](size_t index) -> auto& {
        if (result.size() <= index) {
            result.resize(index + 1);
        }
        return result[index];
    };
    for (auto& inst: BB) {
        size_t rank = ranges::accumulate(inst.operands(), size_t{ 0 },
                                         [&](size_t acc, Value const* operand) {
            return std::max(acc, ranks[operand] + 1);
        });
        for (auto* dest: inst.destRegisters()) {
            ranks[dest] = rank;
        }
        get(rank).insert(&inst);
    }
    return result;
}
