#ifndef SCATHA_IR_TYPES_H_
#define SCATHA_IR_TYPES_H_

#include <span>
#include <string>

#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::ir {

/// Base class of all types in the IR
class Type {
public:
    enum Category { Void, Integral, FloatingPoint, Structure, Function };

    explicit Type(std::string name, Category category): _name(std::move(name)), _category(category) {}

    std::string_view name() const { return _name; }

    auto category() const { return _category; }

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

private:
    std::string _name;
    Category _category;
};

class Arithmetic: public Type {
public:
    size_t bitWidth() const { return _bitWidth; }

protected:
    explicit Arithmetic(std::string_view typenamePrefix, Type::Category category, size_t bitWidth):
        Type(utl::strcat(typenamePrefix, bitWidth), category), _bitWidth(bitWidth) {}

private:
    size_t _bitWidth;
};

class Integral: public Arithmetic {
public:
    explicit Integral(size_t bitWidth): Arithmetic("i", Type::Category::Integral, bitWidth) {}
};

class FloatingPoint: public Arithmetic {
public:
    explicit FloatingPoint(size_t bitWidth): Arithmetic("f", Type::Category::FloatingPoint, bitWidth) {}
};

class StructureType: public Type {
public:
    StructureType(std::string name): Type(std::move(name), Type::Category::Structure) {}

    explicit StructureType(std::string name, std::span<Type const* const> members):
        Type(std::move(name), Type::Category::Structure), _members(members) {}

    Type const* memberAt(std::size_t index) const { return _members[index]; }

    std::span<Type const* const> members() const { return _members; }

    void addMember(Type const* type) { _members.push_back(type); }

private:
    utl::small_vector<Type const*> _members;
};

class FunctionType: public Type {
public:
    explicit FunctionType(Type const* returnType, std::span<Type const* const> parameterTypes):
        Type(makeName(returnType, parameterTypes), Type::Category::Function), _parameterTypes(parameterTypes) {}

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
