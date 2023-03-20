// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_TYPES_H_
#define SCATHA_IR_TYPES_H_

#include <span>
#include <string>

#include <utl/functional.hpp> // For ceil_divide
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/IR/Common.h>

namespace scatha::ir {

/// Base class of all types in the IR
class Type {
public:
    static constexpr size_t invalidSize() { return ~size_t(0); }

    explicit Type(std::string name, TypeCategory category):
        Type(std::move(name), category, invalidSize(), invalidSize()) {}

    explicit Type(std::string name,
                  TypeCategory category,
                  size_t size,
                  size_t align):
        _name(std::move(name)),
        _category(category),
        _size(size),
        _align(align) {}

    std::string_view name() const { return _name; }

    size_t size() const { return _size; }
    size_t align() const { return _align; }

    auto category() const { return _category; }

    struct Hash {
        using is_transparent = void;
        size_t operator()(Type const* type) const {
            return std::hash<std::string_view>{}(type->name());
        }
        size_t operator()(std::string_view name) const {
            return std::hash<std::string_view>{}(name);
        }
    };

    struct Equals {
        using is_transparent = void;
        bool operator()(Type const* lhs, Type const* rhs) const {
            return lhs->name() == rhs->name();
        }
        bool operator()(std::string_view lhs, Type const* rhs) const {
            return lhs == rhs->name();
        }
    };

protected:
    void setSize(size_t size) { _size = size; }
    void setAlign(size_t align) { _align = align; }

private:
    /// For `UniquePtr<>`
    friend void scatha::internal::privateDelete(Type* type);
    
    /// For `ir::DynAllocator`
    friend void scatha::internal::privateDestroy(Type* type);
    
private:
    std::string _name;
    TypeCategory _category;
    size_t _size, _align;
};

/// For dyncast compatibilty of the type category hierarchy
inline TypeCategory dyncast_get_type(std::derived_from<Type> auto const& type) {
    return type.category();
}

/// Represents the void type. This is empty.
class VoidType: public Type {
public:
    VoidType(): Type("void", TypeCategory::VoidType, 0, 0) {}
};

/// Represents a pointer type. Pointers are typed so they know what type they
/// point to.
class PointerType: public Type {
public:
    PointerType():
        Type("ptr",
             TypeCategory::PointerType,
             8, /// For now, maybe we want to derive size and align from
                /// something in the future.
             8) {}
};

/// Base class of `Integral` and `FloatingPoint` types.
class ArithmeticType: public Type {
public:
    size_t bitWidth() const { return _bitWidth; }

protected:
    explicit ArithmeticType(std::string_view typenamePrefix,
                            TypeCategory category,
                            size_t bitWidth):
        Type(utl::strcat(typenamePrefix, bitWidth),
             category,
             utl::ceil_divide(bitWidth, 8),
             utl::ceil_divide(bitWidth, 8)),
        _bitWidth(bitWidth) {}

private:
    size_t _bitWidth;
};

/// Represents an integral type.
class IntegralType: public ArithmeticType {
public:
    explicit IntegralType(size_t bitWidth):
        ArithmeticType("i", TypeCategory::IntegralType, bitWidth) {}
};

/// Represents a floating point type.
class FloatType: public ArithmeticType {
public:
    explicit FloatType(size_t bitWidth):
        ArithmeticType("f", TypeCategory::FloatType, bitWidth) {}
};

/// Represents a (user defined) structure type.
class StructureType: public Type {
public:
    explicit StructureType(std::string name):
        StructureType(std::move(name), {}) {}

    explicit StructureType(std::string name,
                           std::span<Type const* const> members):
        Type(std::move(name), TypeCategory::StructureType, 0, 0),
        _members(members) {}

    Type const* memberAt(std::size_t index) const { return _members[index]; }

    size_t memberOffsetAt(std::size_t index) const {
        return _memberOffsets[index];
    }

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
    explicit FunctionType(Type const* returnType,
                          std::span<Type const* const> parameterTypes):
        Type(makeName(returnType, parameterTypes),
             TypeCategory::FunctionType,
             0,
             0),
        _parameterTypes(parameterTypes) {}

    Type const* returnType() const { return _returnType; }

    std::span<Type const* const> parameterTypes() const {
        return _parameterTypes;
    }

    Type const* parameterTypeAt(std::size_t index) const {
        return _parameterTypes[index];
    }

private:
    static std::string makeName(Type const* returnType,
                                std::span<Type const* const> parameterTypes);

private:
    Type const* _returnType;
    utl::small_vector<Type const*> _parameterTypes;
};

} // namespace scatha::ir

#endif // SCATHA_IR_TYPES_H_
