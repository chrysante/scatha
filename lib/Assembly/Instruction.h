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

    Value& dest() { return _dest; }
    Value const& dest() const { return _dest; }

    Value const& source() const { return _src; }

    size_t numBytes() const { return _numBytes; }

private:
    SCATHA_TESTAPI void verify();

private:
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

/// Represents an `alloca` instruction.
class AllocaInst: public InstructionBase {
public:
    explicit AllocaInst(RegisterIndex dest, RegisterIndex source):
        _dest(dest), _source(source) {}

    RegisterIndex const& dest() const { return _dest; }
    RegisterIndex const& source() const { return _source; }

private:
    RegisterIndex _dest;
    RegisterIndex _source;
};

/// Represents a `cmp*` instruction.
class CompareInst: public InstructionBase {
public:
    explicit CompareInst(Type type, Value lhs, Value rhs):
        _type(type), _lhs(lhs), _rhs(rhs) {}

    Type type() const { return _type; }

    Value const& lhs() const { return _lhs; }

    Value const& rhs() const { return _rhs; }

private:
    Type _type;
    Value _lhs, _rhs;
};

/// Represents a `test` instruction.
class TestInst: public InstructionBase {
public:
    explicit TestInst(Type type, Value operand): _type(type), _op(operand) {
        SC_ASSERT(type != Type::Float, "Float is invalid for TestInst");
    }

    Type type() const { return _type; }

    Value const& operand() const { return _op; }

private:
    Type _type;
    Value _op;
};

/// Represents a `set*` instruction.
class SetInst: public InstructionBase {
public:
    explicit SetInst(RegisterIndex dest, CompareOperation operation):
        _dest(dest), _op(operation) {}

    RegisterIndex const& dest() const { return _dest; }

    CompareOperation operation() const { return _op; }

private:
    RegisterIndex _dest;
    CompareOperation _op;
};

/// Represents the `lnt` and `bnt` instructions.
class UnaryArithmeticInst: public InstructionBase {
public:
    explicit UnaryArithmeticInst(UnaryArithmeticOperation op,
                                 Type type,
                                 RegisterIndex operand):
        _op(op), _type(type), _operand(operand) {}

    UnaryArithmeticOperation operation() const { return _op; }

    Type type() const { return _type; }

    RegisterIndex const& operand() const { return _operand; }

private:
    UnaryArithmeticOperation _op;
    Type _type;
    RegisterIndex _operand;
};

/// Represents a `add`, `sub`, `mul`, ... etc instruction.
class ArithmeticInst: public InstructionBase {
public:
    explicit ArithmeticInst(ArithmeticOperation op,
                            Type type,
                            Value dest,
                            Value source):
        _op(op), _type(type), _dest(dest), _src(source) {
        verify();
    }

    ArithmeticOperation operation() const { return _op; }

    Type type() const { return _type; }

    Value const& dest() const { return _dest; }

    Value const& source() const { return _src; }

private:
    SCATHA_TESTAPI void verify() const;

private:
    ArithmeticOperation _op;
    Type _type;
    Value _dest, _src;
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
