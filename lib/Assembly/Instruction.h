#ifndef SCATHA_ASSEMBLY_INSTRUCTION_H_
#define SCATHA_ASSEMBLY_INSTRUCTION_H_

#include <string>

#include <utl/variant.hpp>

#include "Assembly/Common.h"
#include "Assembly/Value.h"

namespace scatha::Asm {

/// Base class of all instructions in the assembly module.
class InstructionBase {
protected:
    InstructionBase() = default;
};

/// Represents a `mov` instruction.
class MoveInst: public InstructionBase {
public:
    explicit MoveInst(Value dest, Value source, size_t numBytes):
        _dest(dest), _src(source), _numBytes(numBytes) {
        verify();
    }

    Value dest() const { return _dest; }

    void setDest(Value value) { _dest = value; }

    Value source() const { return _src; }

    size_t numBytes() const { return _numBytes; }

private:
    SCATHA_TESTAPI void verify();

private:
    Value _dest, _src;
    size_t _numBytes;
};

/// Represents a `cmov` instruction.
class CMoveInst: public InstructionBase {
public:
    explicit CMoveInst(CompareOperation condition,
                       Value dest,
                       Value source,
                       size_t numBytes):
        _cond(condition), _dest(dest), _src(source), _numBytes(numBytes) {
        verify();
    }

    CompareOperation condition() const { return _cond; }

    Value dest() const { return _dest; }

    Value source() const { return _src; }

    size_t numBytes() const { return _numBytes; }

private:
    SCATHA_TESTAPI void verify();

private:
    CompareOperation _cond;
    Value _dest, _src;
    size_t _numBytes;
};

/// Represents a `jump` instruction
class JumpInst: public InstructionBase {
public:
    explicit JumpInst(CompareOperation condition, u64 targetLabelID):
        _cond(condition), _target(targetLabelID) {}

    explicit JumpInst(u64 targetLabelID):
        JumpInst(CompareOperation::None, targetLabelID) {}

    CompareOperation condition() const { return _cond; }

    u64 targetLabelID() const { return _target; }

    void setTarget(u64 targetLabelID) { _target = targetLabelID; }

private:
    CompareOperation _cond;
    u64 _target;
};

/// Represents a `call` instruction.
class CallInst: public InstructionBase {
public:
    explicit CallInst(u64 functionLabelID, size_t regPtrOffset):
        _functionID(functionLabelID), _regPtrOffset(regPtrOffset) {}

    u64 functionLabelID() const { return _functionID; }

    size_t regPtrOffset() const { return _regPtrOffset; }

private:
    u64 _functionID;
    u64 _regPtrOffset;
};

/// Represents a `call ext` instruction.
class CallExtInst: public InstructionBase {
public:
    explicit CallExtInst(size_t regPtrOffset, size_t slot, size_t index):
        _regPtrOffset(regPtrOffset), _slot(slot), _index(index) {}

    /// Offset to the register pointer where the function finds it's arguments.
    size_t regPtrOffset() const { return _regPtrOffset; }

    /// Index of the table that the called function sits in.
    size_t slot() const { return _slot; }

    /// Index of the called function in the table.
    size_t index() const { return _index; }

private:
    size_t _regPtrOffset;
    size_t _slot;
    size_t _index;
};

/// Represents a `return` instruction.
class ReturnInst: public InstructionBase {
public:
    ReturnInst() = default;
};

/// Represents a terminator instruction.
class TerminateInst: public InstructionBase {
public:
    TerminateInst() = default;
};

/// Represents the `lincsp` instruction, which loads and increments the stack
/// pointer.
class LIncSPInst: public InstructionBase {
public:
    explicit LIncSPInst(RegisterIndex dest, Value16 offset):
        _dest(dest), _offset(offset) {}

    RegisterIndex dest() const { return _dest; }

    Value16 offset() const { return _offset; }

private:
    RegisterIndex _dest;
    Value16 _offset;
};

/// Represents the `lea` instruction.
class LEAInst: public InstructionBase {
public:
    explicit LEAInst(RegisterIndex dest, MemoryAddress address):
        _dest(dest), _address(address) {}

    RegisterIndex dest() const { return _dest; }

    MemoryAddress address() const { return _address; }

private:
    RegisterIndex _dest;
    MemoryAddress _address;
};

/// Represents a `cmp*` instruction.
class CompareInst: public InstructionBase {
public:
    explicit CompareInst(Type type, Value lhs, Value rhs, size_t width):
        _type(type), _lhs(lhs), _rhs(rhs), _width(width) {}

    Type type() const { return _type; }

    Value lhs() const { return _lhs; }

    Value rhs() const { return _rhs; }

    /// Number of bytes to compare.
    /// Must be either of 1, 2, 4 or 8.
    size_t width() const { return _width; }

private:
    Type _type;
    Value _lhs, _rhs;
    size_t _width;
};

/// Represents a `test` instruction.
class TestInst: public InstructionBase {
public:
    explicit TestInst(Type type, Value operand, size_t width):
        _type(type), _op(operand), _width(width) {
        SC_ASSERT(type != Type::Float, "Float is invalid for TestInst");
    }

    Type type() const { return _type; }

    Value operand() const { return _op; }

    /// Number of bytes to test.
    /// Must be either of of 1, 2, 4 or 8.
    size_t width() const { return _width; }

private:
    Type _type;
    Value _op;
    size_t _width;
};

/// Represents a `set*` instruction.
class SetInst: public InstructionBase {
public:
    explicit SetInst(RegisterIndex dest, CompareOperation operation):
        _dest(dest), _op(operation) {}

    RegisterIndex dest() const { return _dest; }

    CompareOperation operation() const { return _op; }

private:
    RegisterIndex _dest;
    CompareOperation _op;
};

/// Represents the `lnt` and `bnt` instructions.
class UnaryArithmeticInst: public InstructionBase {
public:
    explicit UnaryArithmeticInst(UnaryArithmeticOperation op,
                                 RegisterIndex operand):
        _op(op), _operand(operand) {}

    UnaryArithmeticOperation operation() const { return _op; }

    RegisterIndex operand() const { return _operand; }

private:
    UnaryArithmeticOperation _op;
    RegisterIndex _operand;
};

/// Represents a `add`, `sub`, `mul`, ... etc instruction.
class ArithmeticInst: public InstructionBase {
public:
    explicit ArithmeticInst(ArithmeticOperation op,
                            Value dest,
                            Value source,
                            size_t width):
        _op(op), _dest(dest), _src(source), _width(width) {
        verify();
    }

    /// \Returns The arithmetic operation to perform.
    ArithmeticOperation operation() const { return _op; }

    /// \Returns The destination (LHS) operand.
    Value dest() const { return _dest; }

    /// \Returns The source (RHS) operand.
    Value source() const { return _src; }

    /// \Returns The width of the operands in bytes.
    size_t width() const { return _width; }

private:
    SCATHA_TESTAPI void verify() const;

private:
    ArithmeticOperation _op;
    Value _dest, _src;
    size_t _width;
};

/// Represents the `sext*`, `trunc*`, `fext` and `ftrunc`  instructions.
class ConvInst: public InstructionBase {
public:
    explicit ConvInst(RegisterIndex op, Type type, size_t fromBits):
        _op(op), _type(type), _fromBits(fromBits) {
        verify();
    }

    RegisterIndex operand() const { return _op; }

    Type type() const { return _type; }

    size_t fromBits() const { return _fromBits; }

private:
    SCATHA_TESTAPI void verify();

    RegisterIndex _op;
    Type _type;
    size_t _fromBits;
};

namespace internal {

using InstructionVariantBase = utl::cbvariant<InstructionBase,
#define SC_ASM_INSTRUCTION_DEF(inst) inst
#define SC_ASM_INSTRUCTION_SEPARATOR ,
#include "Assembly/Lists.def"
                                              >;

} // namespace internal

/// Represents any concrete instruction.
class Instruction: public internal::InstructionVariantBase {
public:
    using internal::InstructionVariantBase::InstructionVariantBase;
    InstructionType instructionType() const {
        return static_cast<InstructionType>(index());
    }
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_INSTRUCTION_H_
