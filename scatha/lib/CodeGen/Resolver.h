#ifndef SCATHA_CODEGEN_RESOLVER_H_
#define SCATHA_CODEGEN_RESOLVER_H_

#include <functional>

#include <utl/function_view.hpp>
#include <utl/hashtable.hpp>

#include "CodeGen/ValueMap.h"
#include "Common/Metadata.h"
#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

/// Resolves values in an MIR function
class Resolver {
public:
    Resolver() = default;

    explicit Resolver(mir::Context& ctx, mir::Module& mod, mir::Function& F,
                      ValueMap& valueMap,
                      std::function<void(mir::Instruction*)> instEmitter):
        ctx(&ctx),
        mod(&mod),
        F(&F),
        _valueMap(&valueMap),
        instEmitter(std::move(instEmitter)) {}

    /// Maps the IR value \p value to the corresponding MIR value. In particular
    /// - Instructions are mapped to the register(s) that they shall define,
    /// - Global variables are mapped to ...
    /// - Contants are mapped to constants
    /// - `undef` is mapped to `undef`
    /// Mapped values are cached so every call to this function with the same IR
    /// value will return the same MIR value
    mir::Value* resolve(ir::Value const& value) const {
        return resolveImpl(value);
    }

    /// \overload
    template <std::derived_from<ir::Function> F>
    mir::Function* resolve(F const& key) const {
        return cast<mir::Function*>(resolveImpl(key));
    }

    /// \overload
    template <std::derived_from<ir::BasicBlock> BB>
    mir::BasicBlock* resolve(BB const& key) const {
        return cast<mir::BasicBlock*>(resolveImpl(key));
    }

    /// \overload
    template <std::derived_from<ir::Instruction> I>
    mir::SSARegister* resolve(I const& key) const {
        return cast<mir::SSARegister*>(resolveImpl(key));
    }

    /// Calles `resolve()` and copies the values to a register if it is not
    /// already in one
    mir::SSARegister* resolveToRegister(ir::Value const& value,
                                        Metadata metadata) const;

    /// Generate \p numWords adjacent SSA registers
    mir::SSARegister* nextRegister(size_t numWords = 1) const;

    /// Aquire adjacent registers required to store the MIR value corresponding
    /// to \p value
    mir::SSARegister* nextRegistersFor(ir::Value const& value) const;

    /// \Returns The register after \p dest
    template <typename R, typename Copy = mir::CopyInst>
    R* genCopy(R* dest, mir::Value* source, size_t numBytes,
               Metadata metadata) const;

    /// Same as `genCopy()` but generates `cmov` instructions
    template <typename R, typename Copy = mir::CondCopyInst>
    R* genCondCopy(R* dest, mir::Value* source, size_t numBytes,
                   mir::CompareOperation condition, Metadata metadata) const;

    /// Computes the MIR representation of memory address represented by \p addr
    mir::MemoryAddress computeAddress(ir::Value const& addr, size_t offset,
                                      Metadata metadata) const;

    /// \overload for `offset = 0`
    mir::MemoryAddress computeAddress(ir::Value const& addr,
                                      Metadata metadata) const;

    /// Computes the IR GEP instruction \p gep
    /// The \p offset argument exists to emit adjacent load and store
    /// instructions to load and store values larger than one word
    mir::MemoryAddress computeGEP(ir::GetElementPointer const& gep,
                                  size_t offset = 0) const;

    /// Emit an MIR instruction
    void emit(mir::Instruction* inst) const { instEmitter(inst); }

    ///
    ValueMap& valueMap() const { return *_valueMap; }

    /// Emits copy instructions that copy the value into the dest register for
    /// \p inst
    void mapToValue(ir::Instruction const& inst, mir::Value* value);

private:
    mir::Value* resolveImpl(ir::Value const& value) const;
    mir::Value* impl(ir::Instruction const&) const;
    mir::Value* impl(ir::GlobalVariable const&) const;
    mir::Value* impl(ir::IntegralConstant const&) const;
    mir::Value* impl(ir::FloatingPointConstant const&) const;
    mir::Value* impl(ir::NullPointerConstant const&) const;
    mir::Value* impl(ir::RecordConstant const&) const;
    mir::Value* impl(ir::UndefValue const&) const;
    mir::Value* impl(ir::ForeignFunction const& function) const;
    mir::Value* impl(ir::Value const&) const;

    std::pair<mir::Value*, size_t> computeAddressImpl(ir::Value const& addr,
                                                      size_t offset,
                                                      Metadata metadata) const;

    mir::Register* genCopyImpl(
        mir::Register* dest, mir::Value* source, size_t numBytes,
        utl::function_view<void(mir::Register*, mir::Value*, size_t)>
            insertCallback) const;

    mir::Context* ctx = nullptr;
    mir::Module* mod = nullptr;
    mir::Function* F = nullptr;
    ValueMap* _valueMap = nullptr;
    std::function<void(mir::Instruction*)> instEmitter;
};

} // namespace scatha::cg

// ===-----------------------------------------------------------------------===
// ===--- Inline implementation ---------------------------------------------===
// ===-----------------------------------------------------------------------===

namespace scatha::cg {

template <typename R, typename Copy>
R* Resolver::genCopy(R* dest, mir::Value* source, size_t numBytes,
                     Metadata metadata) const {
    mir::Register* result =
        genCopyImpl(dest, source, numBytes,
                    [&](auto* dest, auto* source, size_t numBytes) {
        emit(new Copy(dest, source, numBytes, metadata));
    });
    /// We `static_cast` and not `cast` because `result` is not a valid pointer
    /// but a pointer one past the end
    return static_cast<R*>(result);
}

template <typename R, typename Copy>
R* Resolver::genCondCopy(R* dest, mir::Value* source, size_t numBytes,
                         mir::CompareOperation condition,
                         Metadata metadata) const {
    mir::Register* result =
        genCopyImpl(dest, source, numBytes,
                    [&](auto* dest, auto* source, size_t numBytes) {
        emit(new Copy(dest, source, numBytes, condition, metadata));
    });
    /// See `static_cast` in `genCopy()`
    return static_cast<R*>(result);
}

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_RESOLVER_H_
