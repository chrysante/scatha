#ifndef SCATHA_MIR_INSTRUCTIONS_H_
#define SCATHA_MIR_INSTRUCTIONS_H_

#include "Common/Ranges.h"
#include "MIR/Instruction.h"

namespace scatha::mir {

/// Abstract base class of all instructions with a single (non-memory) operand
class UnaryInstruction: public Instruction {
public:
    /// \Returns the single operand
    Value* operand() { return operandAt(0); }

    /// \overload
    Value const* operand() const { return operandAt(0); }

protected:
    UnaryInstruction(InstType type,
                     Register* dest,
                     Value* operand,
                     size_t byteWidth,
                     Metadata metadata):
        Instruction(type,
                    dest,
                    1,
                    { operand },
                    byteWidth,
                    std::move(metadata)) {}
};

/// Concrete store instruction
class StoreInst: public Instruction, public MemoryInst<StoreInst, 0, 1> {
public:
    explicit StoreInst(MemoryAddress address,
                       Value* source,
                       size_t byteWidth,
                       Metadata metadata):
        Instruction(InstType::StoreInst,
                    nullptr,
                    0,
                    { address.baseAddress(), address.dynOffset(), source },
                    byteWidth,
                    std::move(metadata)),
        MemoryInst(address.constantData()) {}

    /// The address that is stored to
    using MemoryInst::address;

    /// The value stored to memory
    Value* source() { return operandAt(2); }

    /// \overload
    Value const* source() const { return operandAt(2); }
};

/// Concrete load instruction
class LoadInst: public Instruction, public MemoryInst<StoreInst, 0, 1> {
public:
    explicit LoadInst(Register* dest,
                      MemoryAddress source,
                      size_t byteWidth,
                      Metadata metadata):
        Instruction(InstType::LoadInst,
                    dest,
                    1,
                    { source.baseAddress(), source.dynOffset() },
                    byteWidth,
                    std::move(metadata)),
        MemoryInst(source.constantData()) {}

    /// The address that is loaded from
    using MemoryInst::address;
};

/// Abstract base class of `CopyInst` and `CondCopyInst`
class CopyBase: public UnaryInstruction {
public:
    /// The copied value
    Value* source() { return operand(); }

    /// \overload
    Value const* source() const { return operand(); }

protected:
    using UnaryInstruction::UnaryInstruction;
};

/// Concrete copy instruction
class CopyInst: public CopyBase {
public:
    explicit CopyInst(Register* dest,
                      Value* source,
                      size_t byteWidth,
                      Metadata metadata):
        CopyBase(InstType::CopyInst,
                 dest,
                 source,
                 byteWidth,
                 std::move(metadata)) {}
};

/// Concrete call instruction
class CallInst: public Instruction {
public:
    explicit CallInst(Register* dest,
                      size_t numDests,
                      Value* callee,
                      utl::small_vector<Value*> arguments,
                      Metadata metadata);

    ///
    size_t registerOffset() const { return regOffset; }

    ///
    void setRegisterOffset(size_t offset) { regOffset = offset; }

    /// The actual function parameters. Drops the first operand if this is a
    /// `CallInst`
    std::span<Value* const> arguments() { return operands().subspan(1); }

    /// \overload
    std::span<Value const* const> arguments() const {
        return operands().subspan(1);
    }

    /// \Returns the number of callee registers that this function defines.
    /// In SSA form this is the same as `numDests()`
    size_t numReturnRegisters() const { return numRetRegs; }

    /// The called function or function pointer
    Value* callee() { return operandAt(0); }

    /// \overload
    Value const* callee() const { return operandAt(0); }

private:
    size_t regOffset  : 32 = 0;
    size_t numRetRegs : 32 = 0;
};

/// Concrete cond-copy instruction
class CondCopyInst: public CopyBase {
public:
    CondCopyInst(Register* dest,
                 Value* source,
                 size_t byteWidth,
                 mir::CompareOperation condition,
                 Metadata metadata):
        CopyBase(InstType::CondCopyInst,
                 dest,
                 source,
                 byteWidth,
                 std::move(metadata)),
        _cond(condition) {}

    /// \Returns the condition required for the copy to be performed
    mir::CompareOperation condition() const { return _cond; }

private:
    mir::CompareOperation _cond;
};

/// Concrete LISP (load & increment stack pointer) instruction
class LISPInst: public UnaryInstruction {
public:
    explicit LISPInst(Register* dest, Value* allocSize, Metadata metadata):
        UnaryInstruction(InstType::LISPInst,
                         dest,
                         allocSize,
                         0,
                         std::move(metadata)) {}

    /// \Returns `true` if the size of this allocation is known at compile time
    bool isStack() const;

    /// \Returns the value holding the number of bytes allocated
    Value* allocSize() { return operand(); }

    /// \overload
    Value const* allocSize() const { return operand(); }

    /// \Returns the constant number of bytes allocated if this allocation is
    /// static, null otherwise
    Constant const* constantAllocSize() const;
};

/// Concrete LEA instruction
class LEAInst: public Instruction, public MemoryInst<LEAInst, 0, 1> {
public:
    explicit LEAInst(Register* dest, MemoryAddress addr, Metadata metadata):
        Instruction(InstType::LEAInst,
                    dest,
                    1,
                    { addr.baseAddress(), addr.dynOffset() },
                    0,
                    std::move(metadata)),
        MemoryInst(addr.constantData()) {}

    /// The address being computed
    using MemoryInst::address;
};

/// Concrete compare instruction
class CompareInst: public Instruction {
public:
    explicit CompareInst(Value* LHS,
                         Value* RHS,
                         size_t byteWidth,
                         CompareMode mode,
                         Metadata metadata):
        Instruction(InstType::CompareInst,
                    nullptr,
                    0,
                    { LHS, RHS },
                    byteWidth,
                    std::move(metadata)),
        _mode(mode) {}

    /// LHS operand
    Value* LHS() { return operandAt(0); }

    /// \overload
    Value const* LHS() const { return operandAt(0); }

    /// RHS operand
    Value* RHS() { return operandAt(1); }

    /// \overload
    Value const* RHS() const { return operandAt(1); }

    ///
    CompareMode mode() const { return _mode; }

private:
    CompareMode _mode;
};

/// Concrete test instruction
class TestInst: public UnaryInstruction {
public:
    explicit TestInst(Value* operand,
                      size_t byteWidth,
                      CompareMode mode,
                      Metadata metadata):
        UnaryInstruction(InstType::TestInst,
                         nullptr,
                         operand,
                         byteWidth,
                         std::move(metadata)),
        _mode(mode) {}

    ///
    CompareMode mode() const { return _mode; }

private:
    CompareMode _mode;
};

/// Concrete set instruction
class SetInst: public Instruction {
public:
    explicit SetInst(Register* dest,
                     CompareOperation operation,
                     Metadata metadata):
        Instruction(InstType::SetInst, dest, 1, {}, 0, std::move(metadata)),
        op(operation) {}

    ///
    CompareOperation operation() const { return op; }

private:
    CompareOperation op;
};

/// Concrete unary arithmetic instruction
class UnaryArithmeticInst: public UnaryInstruction {
public:
    explicit UnaryArithmeticInst(Register* dest,
                                 Value* operand,
                                 size_t byteWidth,
                                 UnaryArithmeticOperation operation,
                                 Metadata metadata):
        UnaryInstruction(InstType::UnaryArithmeticInst,
                         dest,
                         operand,
                         byteWidth,
                         std::move(metadata)),
        op(operation) {}

    ///
    UnaryArithmeticOperation operation() const { return op; }

private:
    UnaryArithmeticOperation op;
};

/// Abstract base class of `ValueArithmeticInst` and `LoadArithmeticInst`
class ArithmeticInst: public Instruction {
public:
    ///
    ArithmeticOperation operation() const { return op; }

    /// Left hand side operand. Unlike the RHS operand this is in the base class
    /// because the RHS may be a value or a memory location
    Value* LHS() { return operandAt(0); }

    /// \overload
    Value const* LHS() const { return operandAt(0); }

protected:
    ArithmeticInst(InstType instType,
                   Register* dest,
                   utl::small_vector<Value*> operands,
                   size_t byteWidth,
                   ArithmeticOperation operation,
                   Metadata metadata):
        Instruction(instType,
                    dest,
                    1,
                    std::move(operands),
                    byteWidth,
                    std::move(metadata)),
        op(operation) {}

private:
    ArithmeticOperation op;
};

/// Concrete arithmetic instruction operating on two values (registers or
/// constants)
class ValueArithmeticInst: public ArithmeticInst {
public:
    explicit ValueArithmeticInst(Register* dest,
                                 Value* LHS,
                                 Value* RHS,
                                 size_t byteWidth,
                                 ArithmeticOperation operation,
                                 Metadata metadata):
        ArithmeticInst(InstType::ValueArithmeticInst,
                       dest,
                       { LHS, RHS },
                       byteWidth,
                       operation,
                       std::move(metadata)) {}

    /// RIght hand side operand
    Value* RHS() { return operandAt(1); }

    /// \overload
    Value const* RHS() const { return operandAt(1); }
};

/// Concrete arithmetic instruction operating on a value and a memory location
class LoadArithmeticInst:
    public ArithmeticInst,
    public MemoryInst<LoadArithmeticInst, 1, 2> {
public:
    explicit LoadArithmeticInst(Register* dest,
                                Value* LHS,
                                MemoryAddress RHS,
                                size_t byteWidth,
                                ArithmeticOperation operation,
                                Metadata metadata):
        ArithmeticInst(InstType::LoadArithmeticInst,
                       dest,
                       { LHS, RHS.baseAddress(), RHS.dynOffset() },
                       byteWidth,
                       operation,
                       std::move(metadata)),
        MemoryInst({ RHS.constantData() }) {}

    /// Right hand side memory operand
    MemoryAddress RHS() { return address(); }

    /// \overload
    ConstMemoryAddress RHS() const { return address(); }
};

///
class ConversionInst: public UnaryInstruction {
public:
    explicit ConversionInst(Register* dest,
                            Value* operand,
                            Conversion conv,
                            size_t fromBits,
                            size_t toBits,
                            Metadata metadata);

    ///
    Conversion conversion() const { return conv; }

    ///
    size_t fromBits() const { return _fromBits; }

    ///
    size_t toBits() const { return _toBits; }

private:
    Conversion conv;
    uint16_t _fromBits;
    uint16_t _toBits;
};

/// Abstract base class of `JumpInst`, `CondJumpInst` and `ReturnInst`
class TerminatorInst: public Instruction {
protected:
    TerminatorInst(InstType instType,
                   utl::small_vector<Value*> operands,
                   Metadata metadata):
        Instruction(instType,
                    nullptr,
                    0,
                    std::move(operands),
                    0,
                    std::move(metadata)) {}
};

class JumpBase: public TerminatorInst {
public:
    /// The target basic block or function of this jump
    Value* target() {
        return const_cast<Value*>(static_cast<JumpBase const*>(this)->target());
    }

    /// \overload
    Value const* target() const;

protected:
    JumpBase(InstType instType, Value* target, Metadata metadata);
};

///
class JumpInst: public JumpBase {
public:
    explicit JumpInst(Value* target, Metadata metadata):
        JumpBase(InstType::JumpInst, target, std::move(metadata)) {}
};

///
class CondJumpInst: public JumpBase {
public:
    explicit CondJumpInst(Value* target,
                          CompareOperation condition,
                          Metadata metadata):
        JumpBase(InstType::CondJumpInst, target, std::move(metadata)),
        cond(condition) {}

    /// \Returns the compare flag which must be set for this jump to be
    /// performed
    CompareOperation condition() const { return cond; }

private:
    CompareOperation cond;
};

///
class ReturnInst: public TerminatorInst {
public:
    explicit ReturnInst(utl::small_vector<Value*> operands, Metadata metadata):
        TerminatorInst(InstType::ReturnInst,
                       std::move(operands),
                       std::move(metadata)) {}
};

///
class PhiInst: public Instruction {
public:
    explicit PhiInst(Register* dest,
                     utl::small_vector<Value*> operands,
                     size_t byteWidth,
                     Metadata metadata):
        Instruction(InstType::PhiInst,
                    dest,
                    1,
                    std::move(operands),
                    byteWidth,
                    std::move(metadata)) {}
};

///
class SelectInst: public Instruction {
public:
    explicit SelectInst(Register* dest,
                        Value* thenVal,
                        Value* elseVal,
                        CompareOperation condition,
                        size_t byteWidth,
                        Metadata metadata):
        Instruction(InstType::SelectInst,
                    dest,
                    1,
                    { thenVal, elseVal },
                    byteWidth,
                    std::move(metadata)),
        cond(condition) {}

    /// The value selected if the condition is satisfies
    Value* thenValue() { return operandAt(0); }

    /// \overload
    Value const* thenValue() const { return operandAt(0); }

    /// The value selected if the condition is not satisfied
    Value* elseValue() { return operandAt(1); }

    /// \overload
    Value const* elseValue() const { return operandAt(1); }

    /// The compare flag which must be set for `thenValue()` to be selected
    CompareOperation condition() const { return cond; }

private:
    CompareOperation cond;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_INSTRUCTIONS_H_
