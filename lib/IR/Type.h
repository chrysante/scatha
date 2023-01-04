#ifndef SCATHA_IR_TYPES_H_
#define SCATHA_IR_TYPES_H_

#include <span>
#include <string>

#include <utl/functional.hpp> // For ceil_divide
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::ir {

/// Base class of all types in the IR
class Type {
public:
    static constexpr size_t invalidSize() { return ~size_t(0); }
    
    enum Category { Void, Pointer, Integral, FloatingPoint, Structure, Function };

    explicit Type(std::string name, Category category):
        Type(std::move(name), category, invalidSize(), invalidSize()) {}
    
    explicit Type(std::string name,
                  Category category,
                  size_t size,
                  size_t align):
        _name(std::move(name)), _category(category), _size(size), _align(align) {}

    std::string_view name() const { return _name; }
    
    size_t size() const { return _size; }
    size_t align() const { return _align; }
    
    auto category() const { return _category; }

    bool isVoid() const { return category() == Void; }
    bool isPointer() const { return category() == Pointer; }
    bool isIntegral() const { return category() == Integral; }
    bool isFloat() const { return category() == FloatingPoint; }
    bool isStruct() const { return category() == Structure; }
    bool isFunction() const { return category() == Function; }

    struct Hash {
        using is_transparent = void;
        size_t operator()(Type const* type) const { return std::hash<std::string_view>{}(type->name()); }
        size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
    };

    struct Equals {
        using is_transparent = void;
        bool operator()(Type const* lhs, Type const* rhs) const { return lhs->name() == rhs->name(); }
        bool operator()(std::string_view lhs, Type const* rhs) const { return lhs == rhs->name(); }
    };

protected:
    void setSize(size_t size) { _size = size; }
    void setAlign(size_t align) { _align = align; }
    
private:
    std::string _name;
    Category _category;
    size_t _size, _align;
};

/// Base class of \p Integral and \p FloatingPoint types.
class Arithmetic: public Type {
public:
    size_t bitWidth() const { return _bitWidth; }

protected:
    explicit Arithmetic(std::string_view typenamePrefix, Type::Category category, size_t bitWidth):
        Type(utl::strcat(typenamePrefix, bitWidth),
             category,
             utl::ceil_divide(bitWidth, 8),
             utl::ceil_divide(bitWidth, 8)),
        _bitWidth(bitWidth) {}

private:
    size_t _bitWidth;
};

/// Represents an integral type.
class Integral: public Arithmetic {
public:
    explicit Integral(size_t bitWidth): Arithmetic("i", Type::Category::Integral, bitWidth) {}
};

/// Represents a floating point type.
class FloatingPoint: public Arithmetic {
public:
    explicit FloatingPoint(size_t bitWidth): Arithmetic("f", Type::Category::FloatingPoint, bitWidth) {}
};

/// Represents a (user defined) structure type.
class StructureType: public Type {
public:
    StructureType(std::string name): Type(std::move(name), Type::Category::Structure) {}

    explicit StructureType(std::string name, std::span<Type const* const> members):
        Type(std::move(name), Type::Category::Structure), _members(members) {}

    Type const* memberAt(std::size_t index) const { return _members[index]; }
    
    size_t memberOffsetAt(std::size_t index) const { return _memberOffsets[index]; }

    std::span<Type const* const> members() const { return _members; }

    void addMember(Type const* type) {
        _members.push_back(type);
        computeSizeAndAlign();
    }

private:
    void computeSizeAndAlign();
    
private:
    utl::small_vector<Type const*> _members;
    utl::small_vector<u16> _memberOffsets;
};

/// Represents a function type.
class FunctionType: public Type {
public:
    explicit FunctionType(Type const* returnType, std::span<Type const* const> parameterTypes):
        Type(makeName(returnType, parameterTypes), Type::Category::Function, 0, 0), _parameterTypes(parameterTypes) {}

    Type const* returnType() const { return _returnType; }

    std::span<Type const* const> parameterTypes() const { return _parameterTypes; }

    Type const* parameterTypeAt(std::size_t index) const { return _parameterTypes[index]; }

private:
    static std::string makeName(Type const* returnType, std::span<Type const* const> parameterTypes);

private:
    Type const* _returnType;
    utl::small_vector<Type const*> _parameterTypes;
};

} // namespace scatha::ir

#endif // SCATHA_IR_TYPES_H_
