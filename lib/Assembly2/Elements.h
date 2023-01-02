#ifndef SCATHA_ASSEMBLY2_ELEMENTS_H_
#define SCATHA_ASSEMBLY2_ELEMENTS_H_

#include <concepts>
#include <memory>
#include <string>
#include <string_view>

#include <utl/bit.hpp>
#include <utl/utility.hpp>

#include "Assembly2/Common.h"
#include "Basic/Basic.h"

namespace scatha::asm2 {

/// Base class of all data classes in the assembly module.
class Element {
public:
    ElemType elementType() const { return _type; }
    
protected:
    explicit Element(ElemType type): _type(type) {}
    
private:
    ElemType _type;
};

/// Represents a label.
class Label: public Element {
public:
    enum class FunctionLabelTag {};
    static constexpr FunctionLabelTag FunctionLabel{};
    
    explicit Label(u64 uniqueID, std::string name):
        Element(ElemType::Label), _uniqueID(uniqueID), _name(std::move(name)) {}
    
    explicit Label(u64 uniqueID, std::string name, FunctionLabelTag):
        Element(ElemType::Label), _uniqueID(uniqueID), _name(std::move(name)), _isFunctionLabel(true) {}
    
    u64 uniqueID() const { return _uniqueID; }
    std::string_view name() const { return _name; }
    bool isFunctionLabel() const { return _isFunctionLabel; }
    
private:
    u64 _uniqueID;
    std::string _name;
    bool _isFunctionLabel = false;
};

/// Base class of all instructions in the assembly module.
class Instruction: public Element {
public:
    
protected:
    using Element::Element;
    
private:
    
};

/// Represents a \p mov instruction.
class MoveInst: public Instruction {
public:
    explicit MoveInst(std::unique_ptr<Element> dest, std::unique_ptr<Element> source):
    Instruction(ElemType::MoveInst), _dest(std::move(dest)), _src(std::move(source)) {}
    
    Element const& dest() const { return *_dest; }
    
    Element const& source() const { return *_src; }
    
private:
    std::unique_ptr<Element> _dest, _src;
};

/// Represents a jump instruction
class JumpInst: public Instruction {
public:
    explicit JumpInst(CompareOperation condition, std::unique_ptr<Label> target):
        Instruction(ElemType::JumpInst), _cond(condition), _target(std::move(target)) {}
    
    explicit JumpInst(std::unique_ptr<Label> target):
        JumpInst(CompareOperation::None, std::move(target)) {}
    
    CompareOperation condition() const { return _cond; }
    
    Label const& target() const { return *_target; }
    
    void setTarget(std::unique_ptr<Label> target) { _target = std::move(target); }
    
private:
    CompareOperation _cond;
    std::unique_ptr<Label> _target;
};

/// Represents a call instruction.
class CallInst: public Instruction {
public:
    explicit CallInst(std::unique_ptr<Label> function, u64 regPtrOffset):
        Instruction(ElemType::CallInst), _function(std::move(function)), _regPtrOffset(regPtrOffset) {}
    
    Label const& function() const { return *_function; }
    
    u64 regPtrOffset() const { return _regPtrOffset; }
    
private:
    std::unique_ptr<Label> _function;
    u64 _regPtrOffset;
};

/// Represents a return instruction.
class ReturnInst: public Instruction {
public:
    ReturnInst(): Instruction(ElemType::ReturnInst) {}
};

/// Represents a terminate instruction.
class TerminateInst: public Instruction {
public:
    TerminateInst(): Instruction(ElemType::TerminateInst) {}
};

/// Represents a storeRegAddress instruction.
class StoreRegAddress: public Instruction {
public:
    explicit StoreRegAddress(std::unique_ptr<RegisterIndex> dest, std::unique_ptr<RegisterIndex> source):
        Instruction(ElemType::StoreRegAddress), _dest(std::move(dest)), _source(std::move(source)) {}

    RegisterIndex const& dest() const { return *_dest; }
    RegisterIndex const& source() const { return *_source; }

private:
    std::unique_ptr<RegisterIndex> _dest;
    std::unique_ptr<RegisterIndex> _source;
};

/// Represents a compare instruction.
class CompareInst: public Instruction {
public:
    explicit CompareInst(Type type, std::unique_ptr<Element> lhs, std::unique_ptr<Element> rhs):
        Instruction(ElemType::CompareInst), _type(type), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}
    
    Type type() const { return _type; }
    
    Element const& lhs() const { return *_lhs; }
    
    Element const& rhs() const { return *_rhs; }
    
private:
    Type _type;
    std::unique_ptr<Element> _lhs, _rhs;
};

/// Represents a test instruction.
class TestInst: public Instruction {
public:
    explicit TestInst(Type type, std::unique_ptr<Element> operand):
        Instruction(ElemType::TestInst), _type(type), _op(std::move(operand))
    {
        SC_ASSERT(type != Type::Float, "Float is invalid for TestInst");
    }
    
    Type type() const { return _type; }
    
    Element const& operand() const { return *_op; }
    
private:
    Type _type;
    std::unique_ptr<Element> _op;
};

/// Represents a set\* instruction.
class SetInst: public Instruction {
public:
    explicit SetInst(std::unique_ptr<RegisterIndex> dest, CompareOperation operation):
        Instruction(ElemType::SetInst), _dest(std::move(dest)), _op(operation) {}

    RegisterIndex const& dest() const { return *_dest; }
    
    CompareOperation operation() const { return _op; }
    
private:
    std::unique_ptr<RegisterIndex> _dest;
    CompareOperation _op;
};

/// Represents a \p add, \p sub, \p mul, ... etc instruction.
class ArithmeticInst: public Instruction {
public:
    explicit ArithmeticInst(ArithmeticOperation op, Type type, std::unique_ptr<Element> lhs, std::unique_ptr<Element> rhs):
        Instruction(ElemType::ArithmeticInst), _op(op), _type(type), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}
    
    ArithmeticOperation operation() const { return _op; }
    
    Type type() const { return _type; }
    
    Element const& lhs() const { return *_lhs; }
    
    Element const& rhs() const { return *_rhs; }
    
private:
    ArithmeticOperation _op;
    Type _type;
    std::unique_ptr<Element> _lhs, _rhs;
};

/// Represents a register index.
class RegisterIndex: public Element {
public:
    explicit RegisterIndex(std::integral auto index):
    Element(ElemType::RegisterIndex), _value(utl::narrow_cast<u8>(index)) {}
    
    size_t value() const { return _value; }
    
    void setValue(size_t index) { _value = utl::narrow_cast<u8>(index); }
    
private:
    u8 _value;
};

/// Represents a memory address.
class MemoryAddress: public Element {
public:
    explicit MemoryAddress(std::integral auto regIndex,
                           std::integral auto offset,
                           std::integral auto offsetShift):
        Element(ElemType::MemoryAddress),
        _regIndex(utl::narrow_cast<u8>(regIndex)),
        _offset(utl::narrow_cast<u8>(offset)),
        _offsetShift(utl::narrow_cast<u8>(offsetShift)) {}
    
    size_t registerIndex() const { return _regIndex; }
    size_t offset() const { return _offset; }
    size_t offsetShift() const { return _offsetShift; }
    
private:
    u8 _regIndex;
    u8 _offset;
    u8 _offsetShift;
};

/// Base class of all values in the assembly module.
class Value: public Element {
public:
    u64 value() const { return _value; }

protected:
    template <typename T>
    explicit Value(ElemType elemType, utl::tag<T> type, auto value):
        Element(elemType), _value(utl::bit_cast<u64>(widen(utl::narrow_cast<T>(value)))) {}
        
    static f64 widen(std::floating_point auto f) { return f; }
    static i64 widen(std::signed_integral auto i) { return i; }
    static u64 widen(std::unsigned_integral auto i) { return i; }
    
private:
    u64 _value;
};

/// Represents an 8 bit value.
class Value8: public Value {
public:
    explicit Value8(std::signed_integral auto value): Value(ElemType::Value8, utl::tag<i8>{}, value) {}
    explicit Value8(std::unsigned_integral auto value): Value(ElemType::Value8, utl::tag<u8>{}, value) {}
};

/// Represents a 16 bit value.
class Value16: public Value {
public:
    explicit Value16(std::signed_integral auto value): Value(ElemType::Value16, utl::tag<i16>{}, value) {}
    explicit Value16(std::unsigned_integral auto value): Value(ElemType::Value16, utl::tag<u16>{}, value) {}
};

/// Represents a 32 bit value.
class Value32: public Value {
public:
    explicit Value32(std::signed_integral auto value):   Value(ElemType::Value32, utl::tag<i32>{}, value) {}
    explicit Value32(std::unsigned_integral auto value): Value(ElemType::Value32, utl::tag<u32>{}, value) {}
    explicit Value32(f32 value):                         Value(ElemType::Value32, utl::tag<f32>{}, value) {}
};

/// Represents a 64 bit value.
class Value64: public Value {
public:
    explicit Value64(std::signed_integral auto value):   Value(ElemType::Value64, utl::tag<i64>{}, value) {}
    explicit Value64(std::unsigned_integral auto value): Value(ElemType::Value64, utl::tag<u64>{}, value) {}
    explicit Value64(f64 value):                         Value(ElemType::Value64, utl::tag<f64>{}, value) {}
};

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_ELEMENTS_H_

