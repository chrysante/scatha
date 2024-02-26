#include "CodeGen/Resolver.h"

#include <svm/VirtualPointer.h>

#include "CodeGen/ISelCommon.h"
#include "IR/CFG.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;

mir::Value* Resolver::resolveImpl(ir::Value const& value) const {
    if (auto* result = valueMap().getValue(&value)) {
        return result;
    }
    return visit(value, [this](auto& value) { return impl(value); });
}

mir::SSARegister* Resolver::resolveToRegister(ir::Value const& value,
                                              Metadata metadata) const {
    auto* result = resolve(value);
    if (auto* reg = dyncast<mir::SSARegister*>(result)) {
        return reg;
    }
    auto* reg = nextRegister(numWords(value));
    genCopy(reg, result, value.type()->size(), std::move(metadata));
    return reg;
}

mir::SSARegister* Resolver::nextRegister(size_t numWords) const {
    auto* result = new mir::SSARegister();
    F->ssaRegisters().add(result);
    for (size_t i = 1; i < numWords; ++i) {
        auto* r = new mir::SSARegister();
        F->ssaRegisters().add(r);
    }
    return result;
}

mir::SSARegister* Resolver::nextRegistersFor(ir::Value const& value) const {
    return nextRegister(numWords(value));
}

void Resolver::mapToValue(ir::Instruction const& inst, mir::Value* value) {
    genCopy(resolve(inst), value, inst.type()->size(), inst.metadata());
}

mir::Value* Resolver::impl(ir::Instruction const& inst) const {
    if (isa<ir::VoidType>(inst.type())) {
        return nullptr;
    }
    auto* reg = nextRegistersFor(inst);
    valueMap().addValue(&inst, reg);
    return reg;
}

static size_t getOffset(void const* begin, void const* end) {
    SC_EXPECT(begin <= end);
    auto* a = static_cast<char const*>(begin);
    auto* b = static_cast<char const*>(end);
    return static_cast<size_t>(b - a);
}

mir::Value* Resolver::impl(ir::GlobalVariable const& var) const {
    uint64_t const address = [&] {
        if (auto addr = valueMap().getStaticAddress(&var)) {
            return *addr;
        }
        auto* value = var.initializer();
        size_t size = value->type()->size();
        size_t align = value->type()->align();
        auto [data, offset] = mod->allocateStaticData(size, align);
        /// Callback is only executed by function pointers
        auto callback = [&, data = data,
                         offset = offset](ir::Constant const* value,
                                          void* dest) {
            auto* function = cast<ir::Function const*>(value);
            mod->addAddressPlaceholder(offset + getOffset(data, dest),
                                       resolve(*function));
        };
        var.initializer()->writeValueTo(data, callback);
        /// FIXME: Slot index 1 is hard coded here.
        auto address = utl::bit_cast<uint64_t>(
            svm::VirtualPointer{ .offset = offset & 0xFFFF'FFFF'FFFF,
                                 .slotIndex = 1 });
        valueMap().addStaticAddress(&var, address);
        return address;
    }();
    auto* dest = nextRegister();
    emit(new mir::CopyInst(dest, ctx->constant(address, 8), 8, {}));
    return dest;
}

mir::Value* Resolver::impl(ir::IntegralConstant const& constant) const {
    SC_ASSERT(constant.type()->bitwidth() <= 64,
              "Can't handle extended width integers");
    uint64_t value = constant.value().to<uint64_t>();
    auto* mirConst = ctx->constant(value, constant.type()->size());
    valueMap().addValue(&constant, mirConst);
    return mirConst;
}

mir::Value* Resolver::impl(ir::FloatingPointConstant const& constant) const {
    SC_ASSERT(constant.type()->bitwidth() <= 64,
              "Can't handle extended width floats");
    uint64_t value = 0;
    if (constant.value().precision() == APFloatPrec::Single()) {
        value = utl::bit_cast<uint32_t>(constant.value().to<float>());
    }
    else {
        value = utl::bit_cast<uint64_t>(constant.value().to<double>());
    }
    auto* mirConst = ctx->constant(value, constant.type()->size());
    valueMap().addValue(&constant, mirConst);
    return mirConst;
}

mir::Value* Resolver::impl(ir::NullPointerConstant const& constant) const {
    auto* mirConstant = ctx->constant(0, 8);
    valueMap().addValue(&constant, mirConstant);
    return mirConstant;
}

mir::Value* Resolver::impl(ir::RecordConstant const& value) const {
    size_t numWords = ::numWords(value);
    utl::small_vector<uint64_t> words(numWords);
    value.writeValueTo(words.data());
    auto* reg = nextRegister(numWords);
    for (auto* dest = reg; auto word: words) {
        emit(new mir::CopyInst(dest, ctx->constant(word, 8), 8, {}));
        dest = dest->next();
    }
    return reg;
}

mir::Value* Resolver::impl(ir::UndefValue const&) const { return ctx->undef(); }

mir::Value* Resolver::impl(ir::ForeignFunction const& function) const {
    auto* F = new mir::ForeignFunction(function.getFFI());
    mod->addGlobal(F);
    return F;
}

mir::Value* Resolver::impl(ir::Value const&) const {
    SC_UNREACHABLE("Everything else must be manually declared");
}

mir::Register* Resolver::genCopyImpl(
    mir::Register* dest, mir::Value* source, size_t numBytes,
    utl::function_view<void(mir::Register*, mir::Value*, size_t)>
        insertCallback) const {
    size_t const numWords = utl::ceil_divide(numBytes, 8);
    for (size_t i = 0; i < numWords;
         ++i, dest = dest->next(), source = source->next())
    {
        insertCallback(dest, source, sliceWidth(numBytes, i, numWords));
    }
    return dest;
}

std::pair<mir::Value*, size_t> Resolver::computeAddressImpl(
    ir::Value const& addr, size_t offset, Metadata metadata) const {
    auto [base, baseOffset] = valueMap().getAddress(&addr);
    if (base) {
        return { base, baseOffset + offset };
    }
    /// This is kind of ugly because in the worst case we generate many
    /// duplicate copy instruction, but we leave it like this for now. We
    /// generate a copy instruction for every call to this function if the base
    /// address did not resolve to a register, and for big data types this
    /// function will be called in a loop. We leave it like this because if we
    /// cache the result, it would be only computed the last time the address is
    /// used because we select instructions from back to front.
    return { resolveToRegister(addr, metadata), offset };
}

static constexpr size_t ConstantAddressOffsetLimit = 256;

mir::MemoryAddress Resolver::computeAddress(ir::Value const& addr,
                                            size_t offset,
                                            Metadata metadata) const {
    auto [base, totalOffset] = computeAddressImpl(addr, offset, metadata);
    if (totalOffset < ConstantAddressOffsetLimit) {
        return mir::MemoryAddress(base, totalOffset);
    }
    size_t smallOffset = totalOffset % ConstantAddressOffsetLimit;
    size_t bigOffset = totalOffset - smallOffset;
    auto* dynOffset = nextRegister();
    emit(
        new mir::CopyInst(dynOffset, ctx->constant(bigOffset, 8), 8, metadata));
    return mir::MemoryAddress(base, dynOffset,
                              /* offsetFactor = */ 1, smallOffset);
}

mir::MemoryAddress Resolver::computeAddress(ir::Value const& addr,
                                            Metadata metadata) const {
    return computeAddress(addr, 0, std::move(metadata));
}

mir::MemoryAddress Resolver::computeGEP(ir::GetElementPointer const& gep,
                                        size_t userOffset) const {
    auto [base, offset] =
        computeAddressImpl(*gep.basePointer(), userOffset, gep.metadata());
    if (isa<mir::UndefValue>(base)) {
        return mir::MemoryAddress(nullptr, nullptr, 0, 0);
    }
    if (isa<mir::Constant>(base)) {
        SC_UNIMPLEMENTED();
    }
    mir::Register* basereg = cast<mir::Register*>(base);
    auto [dynFactor, constFactor, constOffset] =
        [&, offset = offset]() -> std::tuple<mir::Register*, size_t, size_t> {
        size_t elemSize = gep.inboundsType()->size();
        size_t innerOffset = gep.innerByteOffset() + offset;
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep.arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return { nullptr, elemSize, innerOffset };
        }
        mir::Value* arrayIndex = resolve(*gep.arrayIndex());
        if (auto* regArrayIdx = dyncast<mir::Register*>(arrayIndex)) {
            return { regArrayIdx, elemSize, innerOffset };
        }
        if (auto* constArrIdx = dyncast<mir::Constant*>(arrayIndex)) {
            size_t totalOffset = constArrIdx->value() * elemSize + innerOffset;
            return { nullptr, 0, totalOffset };
        }
        auto* result = nextRegister();
        genCopy(result, arrayIndex, 8, gep.metadata());
        return { result, elemSize, innerOffset };
    }();
    if (constOffset < ConstantAddressOffsetLimit) {
        return mir::MemoryAddress(basereg, dynFactor, constFactor, constOffset);
    }
    if (!dynFactor) {
        size_t smallOffset = constOffset % ConstantAddressOffsetLimit;
        size_t bigOffset = constOffset - smallOffset;
        dynFactor = nextRegister();
        emit(new mir::CopyInst(dynFactor, ctx->constant(bigOffset, 8), 8,
                               gep.metadata()));
        return mir::MemoryAddress(basereg, dynFactor, 1, smallOffset);
    }
    auto* dynFactorSum = nextRegister();
    SC_ASSERT(constFactor <= ConstantAddressOffsetLimit, "");
    size_t bigOffset = constOffset / constFactor;
    size_t smallOffset = constOffset % constFactor;
    emit(new mir::ValueArithmeticInst(dynFactorSum, dynFactor,
                                      ctx->constant(bigOffset, 8), 8,
                                      mir::ArithmeticOperation::Add,
                                      gep.metadata()));
    return mir::MemoryAddress(basereg, dynFactorSum, constFactor, smallOffset);
}
