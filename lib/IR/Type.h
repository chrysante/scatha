// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_TYPES_H_
#define SCATHA_IR_TYPES_H_

#include <span>
#include <string>

#include <utl/functional.hpp> // For ceil_divide
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>

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
    size_t bitwidth() const { return _bitWidth; }

protected:
    explicit ArithmeticType(std::string_view typenamePrefix,
                            TypeCategory category,
                            size_t bitwidth):
        Type(utl::strcat(typenamePrefix, bitwidth),
             category,
             utl::ceil_divide(bitwidth, 8),
             utl::ceil_divide(bitwidth, 8)),
        _bitWidth(bitwidth) {}

private:
    size_t _bitWidth;
};

/// Represents an integral type.
class IntegralType: public ArithmeticType {
public:
    explicit IntegralType(size_t bitwidth):
        ArithmeticType("i", TypeCategory::IntegralType, bitwidth) {}
};

/// Represents a floating point type.
class FloatType: public ArithmeticType {
public:
    explicit FloatType(size_t bitwidth):
        ArithmeticType("f", TypeCategory::FloatType, bitwidth) {}
};

/// Represents a (user defined) structure type.
class SCATHA_TESTAPI StructureType: public Type {
public:
    explicit StructureType(std::string name):
        StructureType(std::move(name), {}) {}

    explicit StructureType(std::string name,
                           std::span<Type const* const> members):
        Type(std::move(name), TypeCategory::StructureType, 0, 0),
        _members(members) {
        computeSizeAndAlign();
    }

    /// \returns The member type at \p index
    Type const* memberAt(std::size_t index) const { return _members[index]; }

    /// \returns The offset of member at \p index in bytes.
    size_t memberOffsetAt(std::size_t index) const {
        return _memberOffsets[index];
    }

    /// \returns A view over the member types in this structure.
    std::span<Type const* const> members() const { return _members; }

    /// Add a member to the end of this structure.
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

/// Represents a fixed size array type.
class SCATHA_TESTAPI ArrayType: public Type {
public:
    explicit ArrayType(Type const* elementType, size_t count);

    /// Element type
    Type const* elementType() const { return _elemType; }

    /// Constant number of element in the array
    size_t count() const { return _count; }

private:
    Type const* _elemType = nullptr;
    size_t _count = 0;
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
