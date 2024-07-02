#include "IRGen/Maps.h"

#include <iostream>
#include <string>

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
        str << printObject(*object) << " -> " << value << "\n";
    }
    str << "\n";
}

void irgen::print(ValueMap const& valueMap) { print(valueMap, std::cout); }

/// # GlobalMap

void GlobalMap::insert(sema::Function const* semaFn,
                       FunctionMetadata metadata) {
    [[maybe_unused]] bool success =
        functions.insert({ semaFn, std::move(metadata) }).second;
    SC_ASSERT(success, "Redeclaration");
}

void GlobalMap::insert(sema::Variable const* semaVar,
                       GlobalVarMetadata metadata) {
    [[maybe_unused]] bool success =
        vars.insert({ semaVar, std::move(metadata) }).second;
    SC_ASSERT(success, "Redeclaration");
}

FunctionMetadata GlobalMap::operator()(sema::Function const* F) const {
    auto result = tryGet(F);
    SC_ASSERT(result, "Not found");
    return *result;
}

std::optional<FunctionMetadata> GlobalMap::tryGet(
    sema::Function const* F) const {
    auto itr = functions.find(F);
    if (itr != functions.end()) {
        return itr->second;
    }
    return std::nullopt;
}

GlobalVarMetadata GlobalMap::operator()(sema::Variable const* V) const {
    auto result = tryGet(V);
    SC_ASSERT(result, "Not found");
    return *result;
}

std::optional<GlobalVarMetadata> GlobalMap::tryGet(
    sema::Variable const* V) const {
    auto itr = vars.find(V);
    if (itr != vars.end()) {
        return itr->second;
    }
    return std::nullopt;
}

/// # TypeMap

template <typename Map>
static void insertImpl(Map& map, typename Map::key_type key,
                       typename Map::mapped_type value) {
    [[maybe_unused]] bool success =
        map.insert({ key, std::move(value) }).second;
    SC_ASSERT(success, "Failed to insert type");
}

void TypeMap::insert(sema::RecordType const* key, ir::StructType const* value,
                     StructMetadata metaData) {
    insertImpl(packedMap, key, value);
    insertImpl(unpackedMap, key, { value });
    if (auto* S = dyncast<sema::StructType const*>(key)) {
        meta.insert({ S, std::move(metaData) });
    }
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
        [&](sema::RecordType const&) {
            SC_UNREACHABLE("Undeclared record type");
        },
        [&](sema::FunctionType const&) {
            SC_UNREACHABLE();
        },
        [&](sema::ArrayType const& type) {
            res = { ctx->arrayType(packed(type.elementType()),
                                   type.count()) };
            if (Repr == Unpacked && type.isDynamic()) {
                res.push_back(ctx->intType(64));
            }
        },
        [&](std::derived_from<sema::PtrRefTypeBase> auto const& type) {
            if (isDynArrayPointer(&type) || isDynArrayReference(&type)) {
                if (Repr == ValueRepresentation::Packed) {
                    res = { makeArrayPtrType(*ctx) };
                }
                else {
                    res = { ctx->ptrType(), ctx->intType(64) };
                }
                return;
            }
            res = { ctx->ptrType() };
            if (type.base().isDyn()) {
                res.push_back(ctx->ptrType());
            }
        },
    }; // clang-format on
    if constexpr (Repr == ValueRepresentation::Packed) {
        SC_ASSERT(res.size() == 1,
                  "Packed types must be represented by one IR type only");
        return res.front();
    }
    else {
        SC_ASSERT(res.size() <= 2,
                  "Unpacked types must be represented by at most two IR types");
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
    return mapChached(packedMap, type,
                      std::bind_front(&TypeMap::compute<Packed>, this));
}

utl::small_vector<ir::Type const*, 2> TypeMap::unpacked(
    sema::Type const* type) const {
    return mapChached(unpackedMap, type,
                      std::bind_front(&TypeMap::compute<Unpacked>, this));
}

utl::small_vector<ir::Type const*, 2> TypeMap::unpacked(
    sema::QualType type) const {
    auto types = unpacked(type.get());
    if (type.isDyn()) {
        SC_ASSERT(types.size() == 1, "");
        types.push_back(ctx->ptrType());
    }
    return types;
}

utl::small_vector<ir::Type const*, 2> TypeMap::map(
    ValueRepresentation repr, sema::Type const* type) const {
    switch (repr) {
    case Packed:
        return { packed(type) };
    case Unpacked:
        return unpacked(type);
    }
}

StructMetadata const& TypeMap::metaData(sema::Type const* type) const {
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

ir::Visibility irgen::mapVisibility(sema::Entity const* entity) {
    /// Only public entities can be `external`
    if (!entity->isPublic()) {
        return ir::Visibility::Internal;
    }
    auto* parent = entity->parent();
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

std::optional<ir::Conversion> irgen::mapArithmeticConv(
    sema::ObjectTypeConversion conv) {
    SC_EXPECT(isArithmeticConversion(conv));
    using enum sema::ObjectTypeConversion;
    switch (conv) {
    case IntTruncTo8:
        return ir::Conversion::Trunc;
    case IntTruncTo16:
        return ir::Conversion::Trunc;
    case IntTruncTo32:
        return ir::Conversion::Trunc;
    case SignedWidenTo16:
        return ir::Conversion::Zext;
    case SignedWidenTo32:
        return ir::Conversion::Zext;
    case SignedWidenTo64:
        return ir::Conversion::Zext;
    case UnsignedWidenTo16:
        return ir::Conversion::Sext;
    case UnsignedWidenTo32:
        return ir::Conversion::Sext;
    case UnsignedWidenTo64:
        return ir::Conversion::Sext;
    case FloatTruncTo32:
        return ir::Conversion::Ftrunc;
    case FloatWidenTo64:
        return ir::Conversion::Fext;
    case SignedToUnsigned:
        return std::nullopt;
    case UnsignedToSigned:
        return std::nullopt;
    case SignedToFloat32:
        return ir::Conversion::StoF;
    case SignedToFloat64:
        return ir::Conversion::StoF;
    case UnsignedToFloat32:
        return ir::Conversion::UtoF;
    case UnsignedToFloat64:
        return ir::Conversion::UtoF;
    case FloatToSigned8:
        return ir::Conversion::FtoS;
    case FloatToSigned16:
        return ir::Conversion::FtoS;
    case FloatToSigned32:
        return ir::Conversion::FtoS;
    case FloatToSigned64:
        return ir::Conversion::FtoS;
    case FloatToUnsigned8:
        return ir::Conversion::FtoU;
    case FloatToUnsigned16:
        return ir::Conversion::FtoU;
    case FloatToUnsigned32:
        return ir::Conversion::FtoU;
    case FloatToUnsigned64:
        return ir::Conversion::FtoU;
    case IntToByte:
        return std::nullopt;
    case ByteToSigned:
        return std::nullopt;
    case ByteToUnsigned:
        return std::nullopt;
    default:
        SC_UNREACHABLE();
    }
}

std::string irgen::arithmeticConvName(sema::ObjectTypeConversion) {
    return "conv";
}
