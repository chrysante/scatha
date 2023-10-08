#include "IRGen/Maps.h"

#include <iostream>
#include <string>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IRGen/Utility.h"
#include "IRGen/Value.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

/// # ValueMap

ValueMap::ValueMap(ir::Context& ctx): ctx(&ctx) {}

void ValueMap::insert(sema::Object const* object, Value value) {
    [[maybe_unused]] bool success = tryInsert(object, value);
    SC_ASSERT(success, "Redeclaration");
}

bool ValueMap::tryInsert(sema::Object const* object, Value value) {
    return values.insert({ object, value }).second;
}

void ValueMap::insertArraySize(sema::Object const* object, Value size) {
    insertArraySize(object, [size] { return size; });
}

void ValueMap::insertArraySize(sema::Object const* object, size_t size) {
    using enum ValueLocation;
    insertArraySize(object, Value(ctx->intConstant(size, 64), Register));
}

void ValueMap::insertArraySize(sema::Object const* object, LazyArraySize size) {
    SC_ASSERT(object, "Must not be null");
    auto [itr, success] = arraySizes.insert({ object, std::move(size) });
    SC_ASSERT(success, "ID already present");
}

void ValueMap::insertArraySizeOf(sema::Object const* newObj,
                                 sema::Object const* original) {
    auto itr = arraySizes.find(original);
    if (itr != arraySizes.end()) {
        insertArraySize(newObj, itr->second);
    }
}

Value ValueMap::operator()(sema::Object const* object) const {
    auto itr = values.find(object);
    SC_ASSERT(itr != values.end(), "Not found");
    return itr->second;
}

std::optional<Value> ValueMap::tryGet(sema::Object const* object) const {
    auto itr = values.find(object);
    if (itr != values.end()) {
        return itr->second;
    }
    return std::nullopt;
}

Value ValueMap::arraySize(sema::Object const* object) const {
    auto result = tryGetArraySize(object);
    SC_ASSERT(result, "Not found");
    return *result;
}

std::optional<Value> ValueMap::tryGetArraySize(
    sema::Object const* object) const {
    /// For statically sized arrays we just extract the size information from
    /// the type, so we don't need to store it in the map
    if (auto size = getStaticArraySize(object->type())) {
        return Value(ctx->intConstant(*size, 64), ValueLocation::Register);
    }
    auto itr = arraySizes.find(object);
    if (itr != arraySizes.end()) {
        return itr->second();
    }
    return std::nullopt;
}

static std::string scopedName(sema::Entity const& entity) {
    std::string name(entity.name());
    auto* parent = entity.parent();
    while (!isa_or_null<sema::GlobalScope>(parent)) {
        name = utl::strcat(parent->name(), ".", name);
        parent = parent->parent();
    }
    return name;
}

static constexpr utl::streammanip printObject =
    [](std::ostream& str, sema::Object const& obj) {
    // clang-format off
    SC_MATCH (obj) {
        [&](sema::Object const& obj) {
            str << scopedName(obj);
        },
        [&](sema::Temporary const& anon) {
            str << "Tmp[" << anon.id() << "]";
        }
    }; // clang-format on
};

void irgen::print(ValueMap const& valueMap, std::ostream& str) {
    for (auto& [object, value]: valueMap) {
        str << printObject(*object) << " -> ";
        ir::printDecl(*value.get(), str);
        str << " "
            << tfmt::format(tfmt::BrightGrey, "[", value.location(), "]");
        if (value.location() == ValueLocation::Memory) {
            str << " // " << *value.type();
        }
        str << "\n";
    }
    str << "\n";
}

void irgen::print(ValueMap const& valueMap) { print(valueMap, std::cout); }

/// # FunctionMap

void FunctionMap::insert(sema::Function const* semaFn,
                         ir::Callable* irFn,
                         FunctionMetaData metaData) {
    bool success = functions.insert({ semaFn, irFn }).second;
    SC_ASSERT(success, "Redeclaration");
    functionMetaData.insert({ semaFn, std::move(metaData) });
}

ir::Callable* FunctionMap::operator()(sema::Function const* function) const {
    auto* result = tryGet(function);
    SC_ASSERT(result, "Not found");
    return result;
}

ir::Callable* FunctionMap::tryGet(sema::Function const* function) const {
    auto itr = functions.find(function);
    if (itr != functions.end()) {
        return itr->second;
    }
    return nullptr;
}

FunctionMetaData const& FunctionMap::metaData(
    sema::Function const* function) const {
    auto itr = functionMetaData.find(function);
    SC_ASSERT(itr != functionMetaData.end(), "Not found");
    return itr->second;
}

/// # TypeMap

void TypeMap::insert(sema::StructType const* key,
                     ir::StructType const* value,
                     StructMetaData metaData) {
    insertImpl(key, value);
    meta.insert({ key, std::move(metaData) });
}

ir::Type const* TypeMap::operator()(sema::QualType type) const {
    return (*this)(static_cast<sema::Type const*>(type.get()));
}

ir::Type const* TypeMap::operator()(sema::Type const* type) const {
    auto itr = map.find(type);
    if (itr != map.end()) {
        return itr->second;
    }
    auto* result = get(type);
    insertImpl(type, result);
    return result;
}

sema::ObjectType const* TypeMap::operator()(ir::Type const* type) const {
    auto itr = backMap.find(type);
    if (itr != backMap.end()) {
        return cast<sema::ObjectType const*>(itr->second);
    }
    return nullptr;
}

StructMetaData const& TypeMap::metaData(sema::Type const* type) const {
    auto itr = meta.find(cast<sema::StructType const*>(type));
    SC_ASSERT(itr != meta.end(), "Not found");
    return itr->second;
}

void TypeMap::insertImpl(sema::Type const* key, ir::Type const* value) const {
    [[maybe_unused]] bool success = map.insert({ key, value }).second;
    SC_ASSERT(success, "Failed to insert type");
    backMap.insert({ value, key });
}

ir::Type const* TypeMap::get(sema::Type const* type) const {
    // clang-format off
    return SC_MATCH (*type) {
        [&](sema::VoidType const&) -> ir::Type const* {
            return ctx->voidType();
        },
        [&](sema::BoolType const&) -> ir::Type const* {
            return ctx->intType(1);
        },
        [&](sema::ByteType const&) -> ir::Type const* {
            return ctx->intType(8);
        },
        [&](sema::IntType const& intType) -> ir::Type const* {
            return ctx->intType(intType.bitwidth());
        },
        [&](sema::FloatType const& floatType) -> ir::Type const* {
            return ctx->floatType(floatType.bitwidth());
        },
        [&](sema::NullPtrType const&) -> ir::Type const* {
            return ctx->ptrType();
        },
        [&](sema::StructType const& structType) -> ir::Type const* {
            SC_UNREACHABLE("Undeclared structure type");
        },
        [&](sema::ArrayType const& arrayType) -> ir::Type const* {
            return ctx->arrayType((*this)(arrayType.elementType()),
                                  arrayType.count());
        },
        [&](sema::PointerType const&) -> ir::Type const* {
            return ctx->ptrType();
        },
        [&](sema::ReferenceType const&) -> ir::Type const* {
            return ctx->ptrType();
        },
    }; // clang-format on
}

ir::UnaryArithmeticOperation irgen::mapUnaryOp(ast::UnaryOperator op) {
    switch (op) {
    case ast::UnaryOperator::BitwiseNot:
        return ir::UnaryArithmeticOperation::BitwiseNot;
    case ast::UnaryOperator::LogicalNot:
        return ir::UnaryArithmeticOperation::LogicalNot;
    default:
        SC_UNREACHABLE();
    }
}

ir::CompareOperation irgen::mapCompareOp(ast::BinaryOperator op) {
    switch (op) {
    case ast::BinaryOperator::Less:
        return ir::CompareOperation::Less;
    case ast::BinaryOperator::LessEq:
        return ir::CompareOperation::LessEq;
    case ast::BinaryOperator::Greater:
        return ir::CompareOperation::Greater;
    case ast::BinaryOperator::GreaterEq:
        return ir::CompareOperation::GreaterEq;
    case ast::BinaryOperator::Equals:
        return ir::CompareOperation::Equal;
    case ast::BinaryOperator::NotEquals:
        return ir::CompareOperation::NotEqual;
    default:
        SC_UNREACHABLE("Only handle compare operations here.");
    }
}

ir::ArithmeticOperation irgen::mapArithmeticOp(sema::BuiltinType const* type,
                                               ast::BinaryOperator op) {
    switch (op) {
    case ast::BinaryOperator::Multiplication:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Mul;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FMul;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case ast::BinaryOperator::Division:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const& type) {
                return type.isSigned() ?
                    ir::ArithmeticOperation::SDiv :
                    ir::ArithmeticOperation::UDiv;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FDiv;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case ast::BinaryOperator::Remainder:
        return cast<sema::IntType const*>(type)->isSigned() ?
                   ir::ArithmeticOperation::SRem :
                   ir::ArithmeticOperation::URem;

    case ast::BinaryOperator::Addition:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Add;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FAdd;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case ast::BinaryOperator::Subtraction:
        // clang-format off
        return visit(*type, utl::overload{
            [](sema::IntType const&) {
                return ir::ArithmeticOperation::Sub;
            },
            [](sema::FloatType const&) {
                return ir::ArithmeticOperation::FSub;
            },
            [](sema::BuiltinType const&) -> ir::ArithmeticOperation {
                SC_UNREACHABLE();
            },
        }); // clang-format on

    case ast::BinaryOperator::LeftShift:
        return ir::ArithmeticOperation::LShL;

    case ast::BinaryOperator::RightShift:
        return ir::ArithmeticOperation::LShR;

    case ast::BinaryOperator::BitwiseAnd:
        return ir::ArithmeticOperation::And;

    case ast::BinaryOperator::BitwiseXOr:
        return ir::ArithmeticOperation::XOr;

    case ast::BinaryOperator::BitwiseOr:
        return ir::ArithmeticOperation::Or;

    default:
        SC_UNREACHABLE("Only handle arithmetic operations here.");
    }
}

ir::ArithmeticOperation irgen::mapArithmeticAssignOp(
    sema::BuiltinType const* type, ast::BinaryOperator op) {
    auto nonAssign = [&] {
        using enum ast::BinaryOperator;
        switch (op) {
        case AddAssignment:
            return Addition;
        case SubAssignment:
            return Subtraction;
        case MulAssignment:
            return Multiplication;
        case DivAssignment:
            return Division;
        case RemAssignment:
            return Remainder;
        case LSAssignment:
            return LeftShift;
        case RSAssignment:
            return RightShift;
        case AndAssignment:
            return BitwiseAnd;
        case OrAssignment:
            return BitwiseOr;
        case XOrAssignment:
            return BitwiseXOr;
        default:
            SC_UNREACHABLE("Only handle arithmetic assign operations here.");
        }
    }();
    return mapArithmeticOp(type, nonAssign);
}

ir::CompareMode irgen::mapCompareMode(sema::BuiltinType const* type) {
    // clang-format off
    return visit(*type, utl::overload{
        [](sema::VoidType const&) -> ir::CompareMode {
            SC_UNREACHABLE();
        },
        [](sema::BoolType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::ByteType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::IntType const& type) {
            return type.isSigned() ?
                ir::CompareMode::Signed :
                ir::CompareMode::Unsigned;
        },
        [](sema::FloatType const&) {
            return ir::CompareMode::Float;
        },
        [](sema::NullPtrType const&) -> ir::CompareMode {
            SC_UNIMPLEMENTED();
        },
    }); // clang-format on
}

ir::FunctionAttribute irgen::mapFuncAttrs(sema::FunctionAttribute attr) {
    switch (attr) {
        using enum ir::FunctionAttribute;
    case sema::FunctionAttribute::Pure:
        return Memory_WriteNone;
    case sema::FunctionAttribute::Const:
        return Memory_ReadNone | Memory_WriteNone;
    default:
        return None;
    }
}

ir::Visibility irgen::mapVisibility(sema::BinaryVisibility spec) {
    switch (spec) {
    case sema::BinaryVisibility::Export:
        return ir::Visibility::External;
    case sema::BinaryVisibility::Internal:
        return ir::Visibility::Internal;
    }
}
