#ifndef SCATHA_ASSEMBLY2_STATEMENTS_H_
#define SCATHA_ASSEMBLY2_STATEMENTS_H_

#include <string>

#include <utl/variant.hpp>

#include "Assembly2/Common.h"
#include "Assembly2/Value.h"

namespace scatha::asm2 {

/// Base class of all instructions in the assembly module.
class InstructionBase {
protected:
    InstructionBase() = default;
    
};

/// Represents a \p mov instruction.
class MoveInst: public InstructionBase {
public:
    explicit MoveInst(Value dest, Value source):
        _dest(dest), _src(source) {}
    
    Value& dest() { return _dest; }
    Value const& dest() const { return _dest; }
    
    Value const& source() const { return _src; }
    
private:
    Value _dest, _src;
};

/// Represents a jump instruction
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

/// Represents a call instruction.
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

/// Represents a return instruction.
class ReturnInst: public InstructionBase {
public:
    ReturnInst() = default;
};

/// Represents a terminate instruction.
class TerminateInst: public InstructionBase {
public:
    TerminateInst() = default;
};

/// Represents an alloca instruction.
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

/// Represents a compare instruction.
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

/// Represents a test instruction.
class TestInst: public InstructionBase {
public:
    explicit TestInst(Type type, Value operand):
        _type(type), _op(operand)
    {
        SC_ASSERT(type != Type::Float, "Float is invalid for TestInst");
    }
    
    Type type() const { return _type; }
    
    Value const& operand() const { return _op; }
    
private:
    Type _type;
    Value _op;
};

/// Represents a set\* instruction.
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

/// Represents the \p lnt and \p bnt instructions.
class UnaryArithmeticInst: public InstructionBase {
public:
    explicit UnaryArithmeticInst(UnaryArithmeticOperation op, Type type, RegisterIndex operand):
        _op(op), _type(type), _operand(operand) {}
    
    UnaryArithmeticOperation operation() const { return _op; }
    
    Type type() const { return _type; }
    
    RegisterIndex const& operand() const { return _operand; }
    
private:
    UnaryArithmeticOperation _op;
    Type _type;
    RegisterIndex _operand;
};

/// Represents a \p add, \p sub, \p mul, ... etc instruction.
class ArithmeticInst: public InstructionBase {
public:
    explicit ArithmeticInst(ArithmeticOperation op, Type type, Value dest, Value source):
        _op(op), _type(type), _dest(dest), _src(source)
    {
        verify();
    }
    
    ArithmeticOperation operation() const { return _op; }
    
    Type type() const { return _type; }
    
    Value const& dest() const { return _dest; }
    
    Value const& source() const { return _src; }
    
private:
    SCATHA(API) void verify() const;
    
private:
    ArithmeticOperation _op;
    Type _type;
    Value _dest, _src;
};

/// Represents a label.
class Label: public InstructionBase {
public:
    explicit Label(u64 id, std::string name): _id(id), _name(std::move(name)) {}
    
    u64 id() const { return _id; }
    
    std::string_view name() const { return _name; }
    
private:
    u64 _id;
    std::string _name;
};


namespace internal {

using InstructionVariantBase = utl::cbvariant<InstructionBase,
#define SC_ASM_INSTRUCTION_DEF(inst) inst
#define SC_ASM_INSTRUCTION_SEPARATOR ,
#include "Assembly2/Lists.def"
>;

} // namespace internal

/// Represents any concrete instruction.
class Instruction: public internal::InstructionVariantBase {
public:
    using internal::InstructionVariantBase::InstructionVariantBase;
    InstructionType instructionType() const { return static_cast<InstructionType>(index()); }
};

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_STATEMENTS_H_
