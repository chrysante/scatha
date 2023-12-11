#include "CodeGen/ISel.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/function_view.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISelCommon.h"
#include "CodeGen/Resolver.h"
#include "CodeGen/SDMatch.h"
#include "CodeGen/SelectionDAG.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

template <>
struct Matcher<ir::Alloca>: MatcherBase {
    SD_MATCH_CASE(ir::Alloca const& inst, SelectionNode& node) {
        auto [addr, offset] = valueMap().getAddress(&inst);
        SC_ASSERT(addr,
                  "Must be set because we handle all allocas in parent "
                  "lowerToMIR()");
        auto* dest = resolve(inst);
        emit(new mir::LEAInst(dest, computeAddress(inst, {}), {}));
        return true;
    }
};

template <>
struct Matcher<ir::Load>: MatcherBase {
    void impl(ir::Load const& load,
              utl::function_view<mir::MemoryAddress(size_t)> addrCallback) {
        auto* dest = resolve(load);
        size_t numBytes = load.type()->size();
        size_t numWords = ::numWords(load);
        for (size_t i = 0; i < numWords; ++i, dest = dest->next()) {
            auto* inst = new mir::LoadInst(dest,
                                           addrCallback(i),
                                           sliceWidth(numBytes, i, numWords),
                                           load.metadata());
            emit(inst);
        }
    }

    // Load -> GEP
    SD_MATCH_CASE(ir::Load const& load, SelectionNode& node) {
        auto* gep = dyncast<ir::GetElementPointer const*>(load.address());
        if (!gep) return false;
        auto* gepNode = DAG(gep);
        if (!gepNode) return false;
        node.merge(*gepNode);
        impl(load, [&](size_t i) { return computeGEP(*gep, i * WordSize); });
        return true;
    }

    // Load
    SD_MATCH_CASE(ir::Load const& load, SelectionNode& node) {
        impl(load, [&](size_t i) {
            return computeAddress(*load.address(),
                                  i * WordSize,
                                  load.metadata());
        });
        return true;
    }
};

template <>
struct Matcher<ir::Store>: MatcherBase {
    void impl(ir::Store const& store,
              utl::function_view<mir::MemoryAddress(size_t)> addrCallback) {
        size_t numBytes = store.value()->type()->size();
        size_t numWords = utl::ceil_divide(numBytes, 8);
        /// We resolve to register because we have no constant -> memory
        /// instructions, so we must take the constant -> register -> memory
        /// detour
        auto* value = resolveToRegister(*store.value(), store.metadata());
        for (size_t i = 0; i < numWords; ++i, value = value->next()) {
            auto* inst = new mir::StoreInst(addrCallback(i),
                                            value,
                                            sliceWidth(numBytes, i, numWords),
                                            store.metadata());
            emit(inst);
        }
    }

    // Store -> GEP
    SD_MATCH_CASE(ir::Store const& store, SelectionNode& node) {
        auto* gep = dyncast<ir::GetElementPointer const*>(store.address());
        if (!gep) return false;
        auto* gepNode = DAG(gep);
        if (!gepNode) return false;
        node.merge(*gepNode);
        impl(store, [&](size_t i) { return computeGEP(*gep, i * WordSize); });
        return true;
    }

    // Store
    SD_MATCH_CASE(ir::Store const& store, SelectionNode& node) {
        impl(store, [&](size_t i) {
            return computeAddress(*store.address(),
                                  i * WordSize,
                                  store.metadata());
        });
        return true;
    }
};

template <>
struct Matcher<ir::ConversionInst>: MatcherBase {
    SD_MATCH_CASE(ir::ConversionInst const& inst, SelectionNode& node) {
        switch (inst.conversion()) {
        case ir::Conversion::Zext:
            [[fallthrough]];
        case ir::Conversion::Trunc:
            [[fallthrough]];
        case ir::Conversion::Bitcast: {
            auto* operand = resolve(*inst.operand());
            if (auto* constant = dyncast<mir::Constant*>(operand)) {
                size_t fromWidth =
                    cast<ir::ArithmeticType const*>(inst.operand()->type())
                        ->bitwidth();
                size_t toWidth =
                    cast<ir::ArithmeticType const*>(inst.type())->bitwidth();
                APInt value = APInt(constant->value(), fromWidth);
                value.zext(toWidth);
                mapToValue(inst,
                           CTX().constant(value.to<uint64_t>(),
                                          utl::ceil_divide(toWidth, 8)));
            }
            else if (auto* undef = dyncast<mir::UndefValue*>(operand)) {
                mapToValue(inst, CTX().undef());
            }
            else {
                SC_ASSERT(isa<mir::Register>(operand), "");
                /// Zext instructions we lower as AND
                if (inst.conversion() == ir::Conversion::Zext) {
                    auto* opType =
                        cast<ir::ArithmeticType const*>(inst.operand()->type());
                    uint64_t mask = ~(~uint64_t(0) << opType->bitwidth());
                    size_t size = inst.type()->size();
                    emit(new mir::ValueArithmeticInst(
                        resolve(inst),
                        operand,
                        CTX().constant(mask, size),
                        size,
                        mir::ArithmeticOperation::And,
                        inst.metadata()));
                }
                /// Everything else is a no-op
                else {
                    mapToValue(inst, operand);
                }
            }
            break;
        }
        case ir::Conversion::Sext:
            [[fallthrough]];
        case ir::Conversion::Fext:
            [[fallthrough]];
        case ir::Conversion::Ftrunc:
            [[fallthrough]];
        case ir::Conversion::UtoF:
            [[fallthrough]];
        case ir::Conversion::StoF:
            [[fallthrough]];
        case ir::Conversion::FtoU:
            [[fallthrough]];
        case ir::Conversion::FtoS: {
            mir::Value* operand = resolve(*inst.operand());
            auto fromBits = utl::narrow_cast<u16>(
                cast<ir::ArithmeticType const*>(inst.operand()->type())
                    ->bitwidth());
            auto toBits = utl::narrow_cast<u16>(
                cast<ir::ArithmeticType const*>(inst.type())->bitwidth());
            emit(new mir::ConversionInst(resolve(inst),
                                         operand,
                                         inst.conversion(),
                                         fromBits,
                                         toBits,
                                         inst.metadata()));
            break;
        }
        case ir::Conversion::_count:
            SC_UNREACHABLE();
        }
        return true;
    }
};

template <>
struct Matcher<ir::CompareInst>: MatcherBase {
    SD_MATCH_CASE(ir::CompareInst const& cmp, SelectionNode& node) {
        auto* LHS = resolveToRegister(*cmp.lhs(), cmp.metadata());
        auto* RHS = resolve(*cmp.rhs());
        emit(new mir::CompareInst(LHS,
                                  RHS,
                                  cmp.lhs()->type()->size(),
                                  cmp.mode(),
                                  cmp.metadata()));
        emit(new mir::SetInst(resolve(cmp), cmp.operation(), cmp.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::UnaryArithmeticInst>: MatcherBase {
    SD_MATCH_CASE(ir::UnaryArithmeticInst const& inst, SelectionNode& node) {
        auto* operand = resolveToRegister(*inst.operand(), inst.metadata());
        emit(new mir::UnaryArithmeticInst(resolve(inst),
                                          operand,
                                          inst.operand()->type()->size(),
                                          inst.operation(),
                                          inst.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::ArithmeticInst>: MatcherBase {
    template <typename InstType>
    void doEmit(ir::ArithmeticInst const& inst, auto RHS) {
        doEmit<InstType>(inst, RHS, inst.operation());
    }

    template <typename InstType>
    void doEmit(ir::ArithmeticInst const& inst,
                auto RHS,
                mir::ArithmeticOperation op) {
        auto* LHS = resolveToRegister(*inst.lhs(), inst.metadata());
        size_t size = inst.lhs()->type()->size();
        if (size < 4) {
            size = 8;
        }
        emit(new InstType(resolve(inst), LHS, RHS, size, op, inst.metadata()));
    }

    /// Tests if the load has no execution dependencies on the LHS value. Only
    /// in that case we can emit a load-arithmetic instruction. Otherwise we
    /// must fall back to seperate instructions
    bool canDeferLoad(ir::Value const* LHS, ir::Load const* load) const {
        auto* LHSInst = dyncast<ir::Instruction const*>(LHS);
        if (!LHSInst) return true;
        return !DAG().dependencies(DAG(LHSInst)).contains(DAG(load));
    }

    // Arithmetic -> Load -> GEP
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load || !canDeferLoad(inst.lhs(), load)) return false;
        auto* loadNode = DAG(load);
        if (!loadNode) return false;
        auto* gep = dyncast<ir::GetElementPointer const*>(load->address());
        if (!gep) return false;
        auto* gepNode = DAG(gep);
        if (!gepNode) return false;
        node.merge(*loadNode);
        node.merge(*gepNode);
        auto RHS = computeGEP(*gep);
        doEmit<mir::LoadArithmeticInst>(inst, RHS);
        return true;
    }

    // Arithmetic -> Load
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load || !canDeferLoad(inst.lhs(), load)) return false;
        auto* loadNode = DAG(load);
        if (!loadNode) return false;
        node.merge(*loadNode);
        auto RHS = computeAddress(*load->address(), load->metadata());
        doEmit<mir::LoadArithmeticInst>(inst, RHS);
        return true;
    }

    /// # Arithmetic patterns representable by LEA

    /// Constants in memory addresses must be representable by one byte
    static constexpr uint64_t LEAConstSizeLimit = 256;

    /// \Returns the value cast to an arithmetic instruction if it is a \p Op
    /// operation, null otherwise
    template <ir::ArithmeticOperation Op>
    static ir::ArithmeticInst const* as(ir::Value const* value) {
        auto* inst = dyncast<ir::ArithmeticInst const*>(value);
        if (inst && inst->operation() == Op) {
            return inst;
        }
        return nullptr;
    }

    /// \Returns the value as a constant if it is smaller than
    /// `LEAConstSizeLimit`
    static std::optional<uint64_t> asLEAConstant(ir::Value const* value) {
        auto* constant = dyncast<ir::IntegralConstant const*>(value);
        if (constant && constant->value().to<uint64_t>() < LEAConstSizeLimit) {
            return constant->value().to<uint64_t>();
        }
        return std::nullopt;
    }

    //    *    Const
    //     \   /
    // *    Mul
    //  \   /                => LEA
    //   Add2  Const
    //     \   /
    //      Add1
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        using enum ir::ArithmeticOperation;
        auto* add1 = as<Add>(&inst);
        if (!add1) return false;
        auto* add2 = as<Add>(add1->lhs());
        if (!add2) return false;
        auto* add2Node = DAG(add2);
        if (!add2Node) return false;
        auto term = asLEAConstant(add1->rhs());
        if (!term) return false;
        auto mul = as<Mul>(add2->rhs());
        if (!mul) return false;
        auto* mulNode = DAG(mul);
        if (!mulNode) return false;
        auto factor = asLEAConstant(mul->rhs());
        if (!factor) return false;
        node.merge(*add2Node);
        node.merge(*mulNode);
        auto* add2LHS = resolveToRegister(*add2->lhs(), add2->metadata());
        auto* mulLHS = resolveToRegister(*mul->lhs(), mul->metadata());
        emit(new mir::LEAInst(resolve(*add1),
                              mir::MemoryAddress(add2LHS,
                                                 mulLHS,
                                                 *factor,
                                                 *term),
                              add1->metadata()));
        return true;
    }

    //    *    Const
    //     \   /
    //      Mul   Const
    //        \   /          => LEA
    //    *    Add2
    //     \   /
    //      Add1
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        using enum ir::ArithmeticOperation;
        auto* add1 = as<Add>(&inst);
        if (!add1) return false;
        auto* add2 = as<Add>(add1->rhs());
        if (!add2) return false;
        auto* add2Node = DAG(add2);
        if (!add2Node) return false;
        auto term = asLEAConstant(add2->rhs());
        if (!term) return false;
        auto mul = as<Mul>(add2->lhs());
        if (!mul) return false;
        auto* mulNode = DAG(mul);
        if (!mulNode) return false;
        auto factor = asLEAConstant(mul->rhs());
        if (!factor) return false;
        node.merge(*add2Node);
        node.merge(*mulNode);
        auto* add1LHS = resolveToRegister(*add1->lhs(), add1->metadata());
        auto* mulLHS = resolveToRegister(*mul->lhs(), mul->metadata());
        emit(new mir::LEAInst(resolve(*add1),
                              mir::MemoryAddress(add1LHS,
                                                 mulLHS,
                                                 *factor,
                                                 *term),
                              add1->metadata()));
        return true;
    }

    //    *    Const
    //     \   /
    // *    Mul              => LEA
    //  \   /
    //   Add
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        using enum ir::ArithmeticOperation;
        auto* add = as<Add>(&inst);
        if (!add) return false;
        auto* mul = as<Mul>(add->rhs());
        if (!mul) return false;
        auto* mulNode = DAG(mul);
        if (!mulNode) return false;
        auto factor = asLEAConstant(mul->rhs());
        if (!factor) return false;
        node.merge(*mulNode);
        auto* addLHS = resolveToRegister(*add->lhs(), add->metadata());
        auto* mulLHS = resolveToRegister(*mul->lhs(), mul->metadata());
        emit(new mir::LEAInst(resolve(*add),
                              mir::MemoryAddress(addLHS, mulLHS, *factor, 0),
                              add->metadata()));
        return true;
    }

    // Arithmetic -> IntConstant
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* constant = dyncast<ir::IntegralConstant const*>(inst.rhs());
        if (!constant) return false;
        APInt rhsVal = constant->value();
        using enum ir::ArithmeticOperation;
        switch (inst.operation()) {
        case Add: {
            if (ucmp(rhsVal, 1) == 0) {
                /// Emit `inc` instruction here
            }
            if (scmp(rhsVal, APInt(-1, rhsVal.bitwidth())) == 0) {
                /// Emit `dec` instruction here
            }
            return false;
        }
        case Sub: {
            if (ucmp(rhsVal, 1) == 0) {
                /// Emit `dec` instruction here
            }
            if (scmp(rhsVal, APInt(-1, rhsVal.bitwidth())) == 0) {
                /// Emit `inc` instruction here
            }
            return false;
        }
        case Mul: {
            if (rhsVal.popcount() != 1) return false;
            doEmit<mir::ValueArithmeticInst>(inst,
                                             CTX().constant(rhsVal.ctz(), 1),
                                             LShL);
            return true;
        }
        case UDiv: {
            if (rhsVal.popcount() != 1) return false;
            doEmit<mir::ValueArithmeticInst>(inst,
                                             CTX().constant(rhsVal.ctz(), 1),
                                             LShR);
            return true;
        }
        case URem: {
            if (rhsVal.popcount() != 1) return false;
            uint64_t mask = ~(~uint64_t(0) << rhsVal.ctz());
            doEmit<mir::ValueArithmeticInst>(
                inst,
                CTX().constant(mask, utl::ceil_divide(rhsVal.bitwidth(), 8)),
                And);
            return true;
        }
        case LShL:
            [[fallthrough]];
        case LShR:
            [[fallthrough]];
        case AShL:
            [[fallthrough]];
        case AShR: {
            /// Shift instructions only allow 8 bit literals as RHS operand.
            doEmit<mir::ValueArithmeticInst>(inst,
                                             CTX().constant(rhsVal
                                                                .to<uint64_t>(),
                                                            1),
                                             inst.operation());
            return true;
        }

        default:
            return false;
        }
    }

    /// \Returns a pair of RHS value, that if it is a constant is adjusted to
    /// legal width and the bytewidth of the instruction
    mir::Value* legalize(ir::ArithmeticInst const& inst) {
        auto* RHS = resolve(*inst.rhs());
        size_t width = inst.lhs()->type()->size();
        if (width >= 4) {
            return RHS;
        }
        if (auto* constant = dyncast<mir::Constant*>(RHS)) {
            return CTX().constant(constant->value(), 8);
        }
        return RHS;
    }

    // Arithmetic (base case)
    SD_MATCH_CASE(ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* RHS = legalize(inst);
        doEmit<mir::ValueArithmeticInst>(inst, RHS);
        return true;
    }
};

template <>
struct Matcher<ir::Goto>: MatcherBase {
    SD_MATCH_CASE(ir::Goto const& gt, SelectionNode& node) {
        auto* target = resolve(*gt.target());
        emit(new mir::JumpInst(target, gt.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::Branch>: MatcherBase {
    void impl(ir::Branch const& br, mir::CompareOperation cond) {
        auto* thenTarget = resolve(*br.thenTarget());
        auto* elseTarget = resolve(*br.elseTarget());
        emit(new mir::CondJumpInst(elseTarget, inverse(cond), br.metadata()));
        emit(new mir::JumpInst(thenTarget, br.metadata()));
    }

    // Branch -> Compare
    SD_MATCH_CASE(ir::Branch const& br, SelectionNode& node) {
        auto* cmp = dyncast<ir::CompareInst const*>(br.condition());
        if (!cmp) return false;
        auto* cmpNode = DAG(cmp);
        if (!cmpNode) return false;
        node.merge(*cmpNode);
        auto* LHS = resolveToRegister(*cmp->lhs(), cmp->metadata());
        auto* RHS = resolve(*cmp->rhs());
        emit(new mir::CompareInst(LHS,
                                  RHS,
                                  cmp->lhs()->type()->size(),
                                  cmp->mode(),
                                  cmp->metadata()));
        impl(br, cmp->operation());
        return true;
    }

    // Branch (base case)
    SD_MATCH_CASE(ir::Branch const& br, SelectionNode& node) {
        /// We can resolve to register without worrying about performance
        /// because if the condition is a constant than it is the optimizers
        /// responsibility to omit the branch
        auto* cond = resolveToRegister(*br.condition(), br.metadata());
        emit(new mir::TestInst(cond,
                               1,
                               mir::CompareMode::Unsigned,
                               br.metadata()));
        impl(br, mir::CompareOperation::NotEqual);
        return true;
    }
};

template <>
struct Matcher<ir::Return>: MatcherBase {
    SD_MATCH_CASE(ir::Return const& ret, SelectionNode& node) {
        utl::small_vector<mir::Value*, 16> args;
        auto* retval = resolve(*ret.value());
        for (size_t i = 0, end = numWords(*ret.value()); i < end;
             ++i, retval = retval->next())
        {
            args.push_back(retval);
        }
        emit(new mir::ReturnInst(std::move(args), ret.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::Call>: MatcherBase {
    SD_MATCH_CASE(ir::Call const& call, SelectionNode& node) {
        utl::small_vector<mir::Value*, 16> args;
        for (auto* arg: call.arguments()) {
            auto* mirArg = resolve(*arg);
            size_t const numWords = ::numWords(*arg);
            for (size_t i = 0; i < numWords; ++i, mirArg = mirArg->next()) {
                args.push_back(mirArg);
            }
        }
        size_t const numDests = numWords(call);
        auto* dest = resolve(call);
        // clang-format off
        SC_MATCH (*call.function()) {
            [&](ir::Value const& func) {
                emit(new mir::CallInst(dest,
                                       numDests,
                                       resolve(func),
                                       std::move(args),
                                       call.metadata()));
            },
            [&](ir::ForeignFunction const& func) {
                emit(new mir::CallExtInst(dest,
                                          numDests,
                                          { .slot  = static_cast<uint32_t>(func.slot()),
                                            .index = static_cast<uint32_t>(func.index()) },
                                          std::move(args),
                                          call.metadata()));
            },
        }; // clang-format on
        return true;
    }
};

template <>
struct Matcher<ir::Phi>: MatcherBase {
    SD_MATCH_CASE(ir::Phi const& phi, SelectionNode& node) {
        auto* dest = resolve(phi);
        auto arguments = phi.arguments() |
                         ranges::views::transform([&](ir::ConstPhiMapping arg) {
                             return resolve(*arg.value);
                         }) |
                         ToSmallVector<>;
        size_t const numBytes = phi.type()->size();
        size_t const numWords = ::numWords(phi);
        for (size_t i = 0; i < numWords; ++i) {
            emit(new mir::PhiInst(dest,
                                  arguments,
                                  sliceWidth(numBytes, i, numWords),
                                  phi.metadata()));
            dest = dest->next();
            ranges::for_each(arguments, [](auto& arg) { arg = arg->next(); });
        }
        return true;
    }
};

template <>
struct Matcher<ir::Select>: MatcherBase {
    void impl(ir::Select const& select, mir::CompareOperation cond) {
        auto* thenVal = resolve(*select.thenValue());
        auto* elseVal = resolve(*select.elseValue());
        size_t numBytes = select.type()->size();
        size_t numWords = utl::ceil_divide(numBytes, 8);
        auto* dest = resolve(select);
        for (size_t i = 0; i < numWords; ++i) {
            emit(new mir::SelectInst(dest,
                                     thenVal,
                                     elseVal,
                                     cond,
                                     sliceWidth(numBytes, i, numWords),
                                     select.metadata()));
            dest = dest->next();
            thenVal = thenVal->next();
            elseVal = elseVal->next();
        }
    }

    // Select -> Compare
    SD_MATCH_CASE(ir::Select const& select, SelectionNode& node) {
        auto* cmp = dyncast<ir::CompareInst const*>(select.condition());
        if (!cmp) return false;
        auto* cmpNode = DAG(cmp);
        if (!cmpNode) return false;
        node.merge(*cmpNode);
        auto* LHS = resolveToRegister(*cmp->lhs(), cmp->metadata());
        auto* RHS = resolve(*cmp->rhs());
        emit(new mir::CompareInst(LHS,
                                  RHS,
                                  cmp->lhs()->type()->size(),
                                  cmp->mode(),
                                  cmp->metadata()));
        impl(select, cmp->operation());
        return true;
    }

    // Select (base case)
    SD_MATCH_CASE(ir::Select const& select, SelectionNode& node) {
        auto* cond = resolveToRegister(*select.condition(), select.metadata());
        emit(new mir::TestInst(cond,
                               1,
                               mir::CompareMode::Unsigned,
                               select.metadata()));
        impl(select, mir::CompareOperation::NotEqual);
        return true;
    }
};

template <>
struct Matcher<ir::GetElementPointer>: MatcherBase {
    SD_MATCH_CASE(ir::GetElementPointer const& gep, SelectionNode& node) {
        mir::MemoryAddress address = computeGEP(gep);
        emit(new mir::LEAInst(resolve(gep), address, gep.metadata()));
        return true;
    }
};

static std::pair<ir::Type const*, size_t> computeInnerTypeAndByteOffset(
    ir::Type const* type, std::span<size_t const> indices) {
    size_t byteOffset = 0;
    for (size_t index: indices) {
        auto* record = cast<ir::RecordType const*>(type);
        byteOffset += record->offsetAt(index);
        type = record->elementAt(index);
    }
    return { type, byteOffset };
}

template <typename R>
static R* advance(R* r, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        r = r->next();
    }
    return r;
}

static uint64_t makeWordMask(size_t leadingZeroBytes, size_t oneBytes) {
    SC_EXPECT(leadingZeroBytes + oneBytes <= 8);
    std::array<uint8_t, 8> mask{};
    for (size_t i = leadingZeroBytes; i < leadingZeroBytes + oneBytes; ++i) {
        mask[i] = 0xFF;
    }
    return utl::bit_cast<uint64_t>(mask);
}

template <>
struct Matcher<ir::ExtractValue>: MatcherBase {
    SD_MATCH_CASE(ir::ExtractValue const& extract, SelectionNode& node) {
        auto* source = resolve(*extract.baseValue());
        if (auto* constant = dyncast<mir::Constant*>(source)) {
            SC_UNIMPLEMENTED();
            return true;
        }
        if (auto* undef = dyncast<mir::UndefValue*>(source)) {
            mapToValue(extract, undef);
            return true;
        }
        mir::Register* srcreg = cast<mir::Register*>(source);
        ir::Type const* const outerType = extract.baseValue()->type();
        auto const [innerType, innerByteBegin] =
            computeInnerTypeAndByteOffset(outerType, extract.memberIndices());
        size_t const innerWordBegin = innerByteBegin / 8;
        size_t const innerByteOffset = innerByteBegin % 8;
        size_t const innerSize = innerType->size();
        srcreg = advance(srcreg, innerWordBegin);
        /// If `innerByteOffset` is 0 i.e. we don't need any bit shifts or
        /// masking, we directly associate the source register with the dest
        /// register.
        if (innerByteOffset == 0) {
            mapToValue(extract, srcreg);
            return true;
        }
        SC_ASSERT(innerByteOffset + innerSize <= 8,
                  "This will need even more work");
        auto* sourceShifted = nextRegister();
        auto* shiftOffset = CTX().constant(8 * innerByteOffset, 1);
        emit(new mir::ValueArithmeticInst(sourceShifted,
                                          srcreg,
                                          shiftOffset,
                                          8,
                                          mir::ArithmeticOperation::LShR,
                                          extract.metadata()));
        auto* sourceMask = CTX().constant(makeWordMask(0, innerSize), 8);
        mir::Register* dest = resolve(extract);
        emit(new mir::ValueArithmeticInst(dest,
                                          sourceShifted,
                                          sourceMask,
                                          8,
                                          mir::ArithmeticOperation::And,
                                          extract.metadata()));
        return true;
    }
};

template <>
struct Matcher<ir::InsertValue>: MatcherBase {
    SD_MATCH_CASE(ir::InsertValue const& insert, SelectionNode& node) {
        auto* insertedMember = resolve(*insert.insertedValue());
        auto* source = resolve(*insert.baseValue());
        auto* dest = resolve(insert);

        /// Slice the outer value like so (`x` marks parts of the inner value,
        /// `_` marks the rest of the outer value, and `outerWordCount` is the
        /// number of words of the outer value):
        /// ```
        ///        ┌─ innerByteOffset // Distance between `innerWordBegin` and
        ///        │                  // `innerByteBegin`
        ///        v
        /// [__|__|_x|xx|xx|xx|xx|xx|x_|__|__]
        ///        ^^                 ^ ^
        ///        │|                 | |
        ///        │└─ innerByteBegin └─┼─ innerByteEnd
        ///        │                    |
        ///        └── innerWordBegin   └─ innerWordEnd
        /// ```
        /// This partitions the outer value into 3 subranges:
        /// `[0, innerWordBegin)` the first words not touching the inner value.
        /// `[innerWordBegin, innerWordEnd)` the first words containing the
        /// inner value.
        /// `[innerWordEnd, outerWordCount)` the last words not touching the
        /// inner value.

        ir::Type const* const outerType = insert.type();
        auto const [innerType, innerByteBegin] =
            computeInnerTypeAndByteOffset(outerType, insert.memberIndices());

        [[maybe_unused]] size_t const innerByteEnd =
            innerByteBegin + innerType->size();
        size_t const innerWordBegin = innerByteBegin / 8;
        size_t const innerWordEnd = innerWordBegin + numWords(*innerType);

        /// Copy the first full words
        dest = genCopy(dest, source, 8 * innerWordBegin, insert.metadata());
        source = advance(source, innerWordBegin);

        /// Handle the complex middle part
        size_t const innerByteOffset = innerByteBegin % 8;
        if (innerByteOffset == 0) {
            /// If we are on a word boundary things are kind of easy.
            /// We emit copies for all full words of the inner value.
            size_t const fullWordsInner = innerType->size() / 8;
            dest = genCopy(dest,
                           insertedMember,
                           8 * fullWordsInner,
                           insert.metadata());
            insertedMember = advance(insertedMember, fullWordsInner);
            source = advance(source, fullWordsInner);
            /// These are the bytes we hang over into the last register of the
            /// inner section.
            size_t const hungOverBytes = innerType->size() % 8;
            if (hungOverBytes != 0) {
                auto* maskedSource = nextRegister();
                auto* sourceMask =
                    CTX().constant(~uint64_t{ 0 } << 8 * hungOverBytes, 8);
                emit(new mir::ValueArithmeticInst(maskedSource,
                                                  source,
                                                  sourceMask,
                                                  8,
                                                  mir::ArithmeticOperation::And,
                                                  insert.metadata()));
                auto* maskedInserted = nextRegister();
                auto* insertedMask = CTX().constant(~sourceMask->value(), 8);
                emit(new mir::ValueArithmeticInst(maskedInserted,
                                                  insertedMember,
                                                  insertedMask,
                                                  8,
                                                  mir::ArithmeticOperation::And,
                                                  insert.metadata()));
                emit(new mir::ValueArithmeticInst(dest,
                                                  maskedSource,
                                                  maskedInserted,
                                                  8,
                                                  mir::ArithmeticOperation::Or,
                                                  insert.metadata()));
                dest = dest->next();
                source = source->next();
            }
        }
        else {
            /// We only handle the case where we need to take care of only one
            /// word.
            SC_ASSERT(innerByteOffset + innerType->size() <= 8,
                      "Everything else is too complex for now");
            auto* shiftCount = CTX().constant(8 * innerByteOffset, 1);
            auto* insertedMask = CTX().constant(
                makeWordMask(/* leadingZeroBytes = */ innerByteOffset,
                             /* oneBytes         = */ innerType->size()),
                8);
            auto* sourceMask = CTX().constant(~insertedMask->value(), 8);
            auto* shiftedInsert = nextRegister();
            emit(new mir::ValueArithmeticInst(shiftedInsert,
                                              insertedMember,
                                              shiftCount,
                                              8,
                                              mir::ArithmeticOperation::LShL,
                                              insert.metadata()));
            auto* maskedSource = nextRegister();
            emit(new mir::ValueArithmeticInst(maskedSource,
                                              source,
                                              sourceMask,
                                              8,
                                              mir::ArithmeticOperation::And,
                                              insert.metadata()));
            auto* maskedInsert = nextRegister();
            emit(new mir::ValueArithmeticInst(maskedInsert,
                                              shiftedInsert,
                                              insertedMask,
                                              8,
                                              mir::ArithmeticOperation::And,
                                              insert.metadata()));
            emit(new mir::ValueArithmeticInst(dest,
                                              maskedSource,
                                              maskedInsert,
                                              8,
                                              mir::ArithmeticOperation::Or,
                                              insert.metadata()));
            dest = dest->next();
            source = source->next();
        }

        /// Copy the last full words
        dest = genCopy(dest,
                       source,
                       utl::round_up(outerType->size(), 8) - 8 * innerWordEnd,
                       insert.metadata());
        (void)dest;
        return true;
    }
};

namespace {

struct ISelBlockCtx {
    SelectionDAG& DAG;

    /// List of instructions that will be populated for every node and then
    /// moved to that node
    List<mir::Instruction> instructions;

    ///
    Resolver resolver;

    /// Tuple of all instruction matchers
    std::tuple<
#define SC_INSTRUCTIONNODE_DEF(Inst, _) Matcher<ir::Inst>,
#include "IR/Lists.def"
        int>
        matchers;

    ISelBlockCtx(SelectionDAG& DAG,
                 mir::Context& ctx,
                 mir::Module& mod,
                 mir::Function& mirFn,
                 ValueMap& map):
        DAG(DAG),
        resolver(ctx, mod, mirFn, map, [this](mir::Instruction* inst) {
            emit(inst);
        }) {
#define SC_INSTRUCTIONNODE_DEF(Inst, _)                                        \
    std::get<Matcher<ir::Inst>>(matchers).init(ctx, DAG, resolver);
#include "IR/Lists.def"
    }

    /// Runs the algorithm
    void run();

    /// Emits the instruction \p inst to the current node. Takes ownership of
    /// the pointer
    void emit(mir::Instruction* inst) { instructions.push_back(inst); }

    /// Sets the computed value and selected instruction to the node
    void finalizeNode(SelectionNode& node);

    /// Meant to be called for every node that shall be lowered
    void match(SelectionNode& node);
};

} // namespace

void cg::isel(SelectionDAG& DAG,
              mir::Context& ctx,
              mir::Module& mod,
              mir::Function& mirFn,
              ValueMap& map) {
    ISelBlockCtx(DAG, ctx, mod, mirFn, map).run();
}

void ISelBlockCtx::run() {
    for (auto* node: DAG.topsort()) {
        if (node->dependentValues().empty() && !DAG.hasSideEffects(node) &&
            !DAG.isOutput(node))
        {
            DAG.erase(node);
            continue;
        }
        match(*node);
    }
}

void ISelBlockCtx::match(SelectionNode& node) {
    auto& inst = *node.irInst();
    bool matched = visit(inst, [&]<typename Inst>(Inst const& inst) {
        return std::get<Matcher<Inst>>(matchers).match(inst, node);
    });
    SC_ASSERT(matched, "Failed to match instruction");
    finalizeNode(node);
}

void ISelBlockCtx::finalizeNode(SelectionNode& node) {
    node.setSelectedInstructions(std::move(instructions));
}
