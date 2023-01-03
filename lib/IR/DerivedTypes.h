#ifndef SCATHA_IR_DERIVEDTYPES_H_
#define SCATHA_IR_DERIVEDTYPES_H_

#include <span>

#include <utl/vector.hpp>

#include "IR/Type.h"

namespace scatha::ir {

/// Class representing built-in types
class FundamentalType: public Type {
public:
    enum ID {
        i8,
        i16,
        i32,
        i64,
        u8,
        u16,
        u32,
        u64,
        f32,
        f64,
    };

    ID id() const { return _id; }

private:
    ID _id;
};

/// Class representing a struct
class StructType: public Type {
public:
    explicit StructType(std::span<Type* const> elementTypes);

    size_t numElements() const { return _fieldTypes.size(); }
    Type* operator[](size_t index) const { return _fieldTypes[index]; }

private:
    utl::small_vector<Type*> _fieldTypes;
};

/// Class representing the type of a function
class FunctionType: public Type {
public:
    explicit FunctionType(std::span<Type* const> argumentTypes, Type* returnType);

private:
    utl::small_vector<Type*> _argumentTypes;
    Type* _returnType;
};

} // namespace scatha::ir

#endif // SCATHA_IR_DERIVEDTYPES_H_
