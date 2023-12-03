#ifndef SCATHA_MIR_INSTRUCTION_H_
#define SCATHA_MIR_INSTRUCTION_H_

#include <span>

#include <utl/vector.hpp>

#include "Common/Metadata.h"
#include "Common/Ranges.h"
#include "Common/UniquePtr.h"
#include "MIR/Fwd.h"
#include "MIR/Register.h"

namespace scatha::mir {

/// Abstract base class of all MIR instructions
class Instruction:
    public ListNode<Instruction>,
    public ParentedNode<BasicBlock>,
    public ObjectWithMetadata {
    static auto destsImpl(auto* self) {
        return ranges::views::generate([dest = self->dest()]() mutable {
                   auto* result = dest;
                   dest = dest->next();
                   return result;
               }) |
               ranges::views::take(self->numDests());
    }

public:
    /// # Queries

    /// \Returns The operand at index \p index
    /// Possibly cast to derived value type `V`
    Value* operandAt(size_t index) {
        return const_cast<Value*>(std::as_const(*this).operandAt(index));
    }

    /// \overload
    Value const* operandAt(size_t index) const { return _ops[index]; }

    /// \Returns A view of pointers to the operands of this instruction
    std::span<Value* const> operands() { return _ops; }

    /// \overload
    std::span<Value const* const> operands() const { return _ops; }

    /// \Returns the number of operands of this instruction
    size_t numOperands() const { return _ops.size(); }

    /// \Returns The (first) register this instruction defines. This instruction
    /// might also define following registers depending on the value of
    /// `numDests()`
    Register* dest() { return _dest; }

    /// \overload
    Register const* dest() const { return _dest; }

    /// \returns the destination register if this instructions defines exactly
    /// one register. Otherwise returns `nullptr`
    Register* singleDest() {
        return const_cast<Register*>(std::as_const(*this).singleDest());
    }

    /// \overload
    Register const* singleDest() const;

    /// View over all destination registers. This is usually just on register
    /// but might be more, for example on `ret` instructions
    auto destRegisters() { return destsImpl(this); }

    /// \overload
    auto destRegisters() const { return destsImpl(this); }

    /// Only applicable for `call` instructions in SSA form.
    /// Hopefully we can generalize this in the future to instructions
    /// which may have multiple (consecutive) dest registers.
    /// This would also be useful for vector instructions.
    size_t numDests() const { return _numDests; }

    /// \Returns The number of bytes this instruction defines
    size_t bytewidth() const { return _byteWidth; }

    /// \Returns The number of bits this instruction defines
    size_t bitwidth() const { return 8 * bytewidth(); }

    /// \Returns The `InstType` enum value of this instruction
    InstType instType() const { return _instType; }

    /// # Modifiers

    /// Set the operands this instruction uses
    void setOperands(utl::small_vector<Value*> operands);

    /// Set the operand at index \p index to \p operand
    /// \pre \p index must be a valid operand index. Internal operand list will
    /// not be resized.
    void setOperandAt(size_t index, Value* operand);

    /// Remove all operands from this instruction
    /// Also clears the internal list of operands (does not set operands
    /// `nullptr`)
    void clearOperands();

    /// Replace all occurences of operand \p old by \p repl
    void replaceOperand(Value* old, Value* repl);

    /// Set the register that this instruction defines to \p dest
    void setDest(Register* dest, size_t numDests);

    /// \overload for `numDests = dest ? 1 : 0`
    void setDest(Register* dest);

    /// This function is used to replace registers as we lower. It does not
    /// modify `numDests`. It only updates the first destination register and
    /// the remaining registers then are the registers following the new first
    /// dest
    void setFirstDest(Register* firstDest);

    /// Set the destination register to `nullptr` and `numDests` to 1
    void clearDest();

    ///
    UniquePtr<Instruction> clone() const;

protected:
    Instruction(InstType instType,
                Register* dest,
                size_t numDests,
                utl::small_vector<Value*> operands,
                size_t byteWidth,
                Metadata metadata);

private:
    InstType _instType;
    Register* _dest = nullptr;
    utl::small_vector<Value*> _ops;
    size_t _numDests = 0;
    size_t _byteWidth;
};

/// For `dyncast` et al to work
inline InstType dyncast_get_type(Instruction const& inst) {
    return inst.instType();
}

/// CRTP base class for all instructions that access memory.
/// This class exists to define a getter for the memory address that the derived
/// instruction accesses
template <typename Derived, size_t AddrIdx, size_t OffsetIdx>
class MemoryInst {
public:
    MemoryAddress address() {
        return { derived().operandAt(AddrIdx),
                 derived().operandAt(OffsetIdx),
                 _constData };
    }

    ConstMemoryAddress address() const {
        return { derived().operandAt(AddrIdx),
                 derived().operandAt(OffsetIdx),
                 _constData };
    }

    void setAddress(MemoryAddress addr) { _constData = addr.constantData(); }

protected:
    MemoryInst(MemAddrConstantData constData): _constData(constData) {}

private:
    Derived& derived() { return static_cast<Derived&>(*this); }
    Derived const& derived() const {
        return static_cast<Derived const&>(*this);
    }

    MemAddrConstantData _constData;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_INSTRUCTION_H_
