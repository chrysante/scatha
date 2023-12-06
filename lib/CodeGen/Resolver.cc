#include "CodeGen/Resolver.h"

#include "CodeGen/ISelCommon.h"
#include "IR/CFG.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Instructions.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;

mir::Value* Resolver::resolveImpl(ir::Value const& value) const {
    if (auto* result = valueMap()(&value)) {
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
    valueMap().insert(&inst, reg);
    return reg;
}

mir::Value* Resolver::impl(ir::GlobalVariable const&) const {
    SC_UNIMPLEMENTED();
}

mir::Value* Resolver::impl(ir::IntegralConstant const& constant) const {
    SC_ASSERT(constant.type()->bitwidth() <= 64,
              "Can't handle extended width integers");
    uint64_t value = constant.value().to<uint64_t>();
    auto* mirConst = ctx->constant(value, constant.type()->size());
    valueMap().insert(&constant, mirConst);
    return mirConst;
}

mir::Value* Resolver::impl(ir::FloatingPointConstant const& constant) const {
    SC_ASSERT(constant.type()->bitwidth() <= 64,
              "Can't handle extended width floats");
    uint64_t value = 0;
    if (constant.value().precision() == APFloatPrec::Single) {
        value = utl::bit_cast<uint32_t>(constant.value().to<float>());
    }
    else {
        value = utl::bit_cast<uint64_t>(constant.value().to<double>());
    }
    auto* mirConst = ctx->constant(value, constant.type()->size());
    valueMap().insert(&constant, mirConst);
    return mirConst;
}

mir::Value* Resolver::impl(ir::NullPointerConstant const& constant) const {
    auto* mirConstant = ctx->constant(0, 8);
    valueMap().insert(&constant, mirConstant);
    return mirConstant;
}

mir::Value* Resolver::impl(ir::RecordConstant const&) const {
    SC_UNIMPLEMENTED();
}

mir::Value* Resolver::impl(ir::UndefValue const&) const { return ctx->undef(); }

mir::Value* Resolver::impl(ir::Value const&) const {
    SC_UNREACHABLE("Everything else must be manually declared");
}

mir::Register* Resolver::genCopyImpl(mir::Register* dest,
                                     mir::Value* source,
                                     size_t numBytes,
                                     Metadata metadata,
                                     auto insertCallback) const {
    size_t const numWords = utl::ceil_divide(numBytes, 8);
    for (size_t i = 0; i < numWords;
         ++i, dest = dest->next(), source = source->next())
    {
        insertCallback(dest, source, sliceWidth(numBytes, i, numWords));
    }
    return dest;
}

mir::MemoryAddress Resolver::computeGEP(ir::GetElementPointer const* gep,
                                        size_t offset) const {
    auto* base = resolve(*gep->basePointer());
    if (auto* undef = dyncast<mir::UndefValue*>(base)) {
        return mir::MemoryAddress(nullptr, nullptr, 0, 0);
    }
    if (auto* constant = dyncast<mir::Constant*>(base)) {
        SC_UNIMPLEMENTED();
    }
    mir::Register* basereg = cast<mir::Register*>(base);
    auto [dynFactor, constFactor, constOffset] =
        [&]() -> std::tuple<mir::Register*, size_t, size_t> {
        size_t elemSize = gep->inboundsType()->size();
        size_t innerOffset = gep->innerByteOffset() + offset;
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep->arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return { nullptr, elemSize, innerOffset };
        }
        mir::Value* arrayIndex = resolve(*gep->arrayIndex());
        if (auto* regArrayIdx = dyncast<mir::Register*>(arrayIndex)) {
            return { regArrayIdx, elemSize, innerOffset };
        }
        if (auto* constArrIdx = dyncast<mir::Constant*>(arrayIndex)) {
            size_t totalOffset = constArrIdx->value() * elemSize + innerOffset;
            // FIXME: This should not be hard coded
            if (totalOffset < 256) {
                return { nullptr, 0, totalOffset };
            }
        }
        auto* result = nextRegister();
        genCopy(result, arrayIndex, 8, gep->metadata());
        return { result, elemSize, innerOffset };
    }();
    return mir::MemoryAddress(basereg, dynFactor, constFactor, constOffset);
}

// We keep this here until we implement it
#if 0
static size_t alignTo(size_t size, size_t align) {
    if (size % align == 0) {
        return size;
    }
    return size + align - size % align;
}
void FunctionContext::computeAllocaMap(mir::BasicBlock& mirEntry) {
    auto& entry = irFn.entry();
    auto allocas = entry | ranges::views::take_while(isa<ir::Alloca>) |
                   ranges::views::transform(cast<ir::Alloca const&>);
    size_t offset = 0;
    utl::small_vector<size_t> offsets;
    /// Compute the offsets for the allocas
    SC_ASSERT(ranges::all_of(allocas, &ir::Alloca::isStatic),
              "For now we only support lowering static allocas");
    for (auto& inst: allocas) {
        offsets.push_back(offset);
        auto size = inst.allocatedSize();
        size_t const StaticAllocaAlign = 16;
        offset += alignTo(*size, StaticAllocaAlign);
    }

    /// Emit one `lincsp` instruction
    mir::Register* baseptr = nullptr; // TODO: Implement this

    /// Store the results in the map
    for (auto [inst, offset]: ranges::views::zip(allocas, offsets)) {
        allocaMap.insert(&inst, { baseptr, offset });
    }
}
#endif
