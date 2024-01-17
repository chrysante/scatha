#include "IRGen/Maps.h"

#include <iostream>
#include <string>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG/Constants.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "IR/Type.h"
#include "IRGen/Utility.h"
#include "IRGen/Value.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;
using enum ValueRepresentation;

/// # ValueMap

ValueMap::ValueMap(ir::Context& ctx): ctx(&ctx) {}

void ValueMap::insert(sema::Object const* obj, Value value) {
    [[maybe_unused]] bool success = tryInsert(obj, std::move(value));
    SC_ASSERT(success, "Redeclaration");
}

bool ValueMap::tryInsert(sema::Object const* obj, Value value) {
    return values.insert({ obj, std::move(value) }).second;
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

static std::string scopedName(sema::Entity const& entity) {
    std::string name(entity.name());
    auto* parent = entity.parent();
    while (!isa<sema::GlobalScope>(parent)) {
        name = utl::strcat(parent->name(), ".", name);
        parent = parent->parent();
    }
    return name;
}

static constexpr utl::streammanip printObject = [](std::ostream& str,
                                                   sema::Object const& obj) {
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
        for (bool first = true; auto* irVal: value.get()) {
            if (!first) {
                str << ", ";
            }
            first = false;
            ir::printDecl(*irVal, str);
        }
        str << " "
            << tfmt::format(tfmt::BrightGrey,
                            "[",
                            value.location(),
                            ", ",
                            value.representation(),
                            "]");
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

template <typename Map>
static void insertImpl(Map& map,
                       typename Map::key_type key,
                       typename Map::mapped_type value) {
    [[maybe_unused]] bool success =
        map.insert({ key, std::move(value) }).second;
    SC_ASSERT(success, "Failed to insert type");
}

void TypeMap::insert(sema::StructType const* key,
                     ir::StructType const* value,
                     StructMetaData metaData) {
    insertImpl(packedMap, key, value);
    insertImpl(unpackedMap, key, { value });
    meta.insert({ key, std::move(metaData) });
}

template <ValueRepresentation Repr>
auto TypeMap::compute(sema::Type const* type) const {
    utl::small_vector<ir::Type const*, 2> res;
    // clang-format off
    SC_MATCH (*type) {
        [&](sema::VoidType const&) { res = { ctx->voidType() }; },
        [&](sema::BoolType const&) { res = { ctx->intType(1) }; },
        [&](sema::ByteType const&) { res = { ctx->intType(8) }; },
        [&](sema::IntType const& intType) {
            res = { ctx->intType(intType.bitwidth()) };
        },
        [&](sema::FloatType const& floatType) {
            res = { ctx->floatType(floatType.bitwidth()) };
        },
        [&](sema::NullPtrType const&) { res = { ctx->ptrType() }; },
        [&](sema::StructType const&) {
            SC_UNREACHABLE("Undeclared structure type");
        },
        [&](sema::FunctionType const&) {
            /// This is unreachable because function types don't translate
            /// easily to IR. IR functions have type `ptr` and store return
            /// and argument types separately
            SC_UNREACHABLE();
        },
        [&](sema::ArrayType const& type) {
//            SC_ASSERT(!type.isDynamic(),
//                      "Cannot represent dynamic arrays as IR types");
            res = { ctx->arrayType(packed(type.elementType()),
                                   type.count()) };
        },
        [&]<typename T>(T const& type)
            requires std::derived_from<T, sema::PointerType> ||
                     std::derived_from<T, sema::ReferenceType>
        {
            if (isFatPointer(&type)) {
                if (Repr == ValueRepresentation::Packed) {
                    res = { makeArrayPtrType(*ctx) };
                }
                else {
                    res = { ctx->ptrType(), ctx->intType(64) };
                }
            }
            else {
                res = { ctx->ptrType() };
            }
        },
    }; // clang-format on
    if constexpr (Repr == ValueRepresentation::Packed) {
        SC_ASSERT(res.size() == 1,
                  "Packed types must be represented by one IR type only");
        return res.front();
    }
    else {
        return res;
    }
}

static auto mapChached(auto& cache, sema::Type const* key, auto compute) {
    auto itr = cache.find(key);
    if (itr != cache.end()) {
        return itr->second;
    }
    auto result = compute(key);
    insertImpl(cache, key, result);
    return result;
}

ir::Type const* TypeMap::packed(sema::Type const* type) const {
    return mapChached(packedMap,
                      type,
                      std::bind_front(&TypeMap::compute<Packed>, this));
}

utl::small_vector<ir::Type const*, 2> TypeMap::unpacked(
    sema::Type const* type) const {
    return mapChached(unpackedMap,
                      type,
                      std::bind_front(&TypeMap::compute<Unpacked>, this));
}

StructMetaData const& TypeMap::metaData(sema::Type const* type) const {
    auto itr = meta.find(cast<sema::StructType const*>(type));
    SC_ASSERT(itr != meta.end(), "Not found");
    return itr->second;
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

ir::ArithmeticOperation irgen::mapArithmeticOp(sema::ObjectType const* type,
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
            [](sema::ObjectType const&) -> ir::ArithmeticOperation {
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
            [](sema::ObjectType const&) -> ir::ArithmeticOperation {
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
            [](sema::ObjectType const&) -> ir::ArithmeticOperation {
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
            [](sema::ObjectType const&) -> ir::ArithmeticOperation {
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
    sema::ObjectType const* type, ast::BinaryOperator op) {
    return mapArithmeticOp(type, toNonAssignment(op));
}

ir::CompareMode irgen::mapCompareMode(sema::ObjectType const* type) {
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
        [](sema::NullPtrType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::PointerType const&) {
            return ir::CompareMode::Unsigned;
        },
        [](sema::CompoundType const&) -> ir::CompareMode {
            SC_UNREACHABLE();
        }
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

ir::Visibility irgen::mapVisibility(sema::Function const* function) {
    /// Only generate public functions can be `external`
    if (!function->isPublic()) {
        return ir::Visibility::Internal;
    }
    auto* parent = function->parent();
    /// Derived functions for array types or unique ptr types are not `external`
    if (isa<sema::Type>(parent) && !isa<sema::StructType>(parent)) {
        return ir::Visibility::Internal;
    }
    return ir::Visibility::External;
}

std::string irgen::binaryOpResultName(ast::BinaryOperator op) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        return "prod";
    case Division:
        return "quot";
    case Remainder:
        return "rem";
    case Addition:
        return "sum";
    case Subtraction:
        return "diff";
    case LeftShift:
        return "lshift";
    case RightShift:
        return "rshift";
    case Less:
        return "ls";
    case LessEq:
        return "lseq";
    case Greater:
        return "grt";
    case GreaterEq:
        return "grteq";
    case Equals:
        return "eq";
    case NotEquals:
        return "neq";
    case BitwiseAnd:
        return "and";
    case BitwiseXOr:
        return "xor";
    case BitwiseOr:
        return "or";
    case LogicalAnd:
        return "land";
    case LogicalOr:
        return "lor";
    case Assignment:
        return "?";
    case AddAssignment:
        return "sum";
    case SubAssignment:
        return "diff";
    case MulAssignment:
        return "prod";
    case DivAssignment:
        return "quot";
    case RemAssignment:
        return "rem";
    case LSAssignment:
        return "lshift";
    case RSAssignment:
        return "rshift";
    case AndAssignment:
        return "and";
    case OrAssignment:
        return "or";
    case XOrAssignment:
        return "xor";
    case Comma:
        return "?";
    }
    SC_UNREACHABLE();
}
