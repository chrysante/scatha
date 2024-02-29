#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>
#include <variant>

#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"
#include "Sema/ThinExpr.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using enum ValueCategory;

/// Converts the non-erroneous `ConvExp<Conv>` \p c to a `std::optional<Conv>`
/// \Returns `std::nullopt` if \p c is a noop or the conversion if \p c holds a
/// conversion
/// \Pre \p c must not hold a conversion error
template <typename Conv>
static std::optional<Conv> toOpt(ConvExp<Conv> c) {
    SC_EXPECT(!c.isError());
    if (c.isNoop()) {
        return std::nullopt;
    }
    return c.value();
}

using ConvChain = utl::small_vector<ObjectTypeConversion, 8>;

static std::optional<ConvChain> implExplIntConversion(ConversionKind kind,
                                                      IntType const& from,
                                                      IntType const& to) {
    SC_EXPECT(kind != ConversionKind::Reinterpret);
    using enum ObjectTypeConversion;
    ConvChain result;
    // clang-format off
    static constexpr ConvExp<ObjectTypeConversion> SignConvMat[2][2] = {
        //             Unsigned          Signed
        /* Unsigned */ { ConvNoop,         UnsignedToSigned },
        /*   Signed */ { SignedToUnsigned, ConvNoop }
    }; // clang-format on
    auto signConv = SignConvMat[from.isSigned()][to.isSigned()];
    if (kind == ConversionKind::Implicit) {
        if (signConv == SignedToUnsigned) {
            return std::nullopt;
        }
        if (signConv == UnsignedToSigned && from.bitwidth() >= to.bitwidth()) {
            return std::nullopt;
        }
    }
    if (signConv.hasValue()) {
        result.push_back(signConv.value());
    }
    if (from.bitwidth() == to.bitwidth()) {
        return result;
    }
    else if (from.bitwidth() > to.bitwidth()) {
        if (kind == ConversionKind::Implicit) {
            return std::nullopt;
        }
        switch (to.bitwidth()) {
        case 8:
            result.push_back(IntTruncTo8);
            break;
        case 16:
            result.push_back(IntTruncTo16);
            break;
        case 32:
            result.push_back(IntTruncTo32);
            break;
        default:
            SC_UNREACHABLE();
        }
        return result;
    }
    else {
        switch (to.bitwidth()) {
        case 16:
            result.push_back(to.isSigned() ? SignedWidenTo16 :
                                             UnsignedWidenTo16);
            break;
        case 32:
            result.push_back(to.isSigned() ? SignedWidenTo32 :
                                             UnsignedWidenTo32);
            break;
        case 64:
            result.push_back(to.isSigned() ? SignedWidenTo64 :
                                             UnsignedWidenTo64);
            break;
        default:
            SC_UNREACHABLE();
        }
        return result;
    }
    return result;
}

static std::optional<ConvChain> implAndExplArrayToArrayConv(
    ArrayType const& from, ArrayType const& to) {
    using enum ObjectTypeConversion;
    if (&from == &to) {
        return ConvChain{};
    }
    if (from.elementType() == to.elementType() && !from.isDynamic() &&
        to.isDynamic())
    {
        return ConvChain{ ArrayRef_FixedToDynamic };
    }
    return std::nullopt;
};

/// Forward declaration here because `pointerConv` uses this recursively
static std::optional<ConvChain> determineObjConv(ConversionKind kind,
                                                 ThinExpr from, ThinExpr to);

/// Maps reference conversions to pointer conversions
static ObjectTypeConversion convRefToPtr(ObjectTypeConversion conv) {
    using enum ObjectTypeConversion;
    switch (conv) {
    case ArrayRef_FixedToDynamic:
        return ArrayPtr_FixedToDynamic;
    case Reinterpret_ValuePtr:
        return Reinterpret_ValueRef;
    case Reinterpret_ValueRef_ToByteArray:
        return Reinterpret_ValuePtr_ToByteArray;
    case Reinterpret_ValueRef_FromByteArray:
        return Reinterpret_ValuePtr_FromByteArray;
    case Reinterpret_DynArrayRef_ToByte:
        return Reinterpret_DynArrayPtr_ToByte;
    case Reinterpret_DynArrayRef_FromByte:
        return Reinterpret_DynArrayPtr_FromByte;
    default:
        SC_UNREACHABLE();
    }
}

static std::optional<ConvChain> pointerConv(ConversionKind kind,
                                            PointerType const& from,
                                            PointerType const& to) {
    using enum ObjectTypeConversion;
    if (from.base().isConst() && to.base().isMut()) {
        return std::nullopt;
    }
    ConvChain result;
    // clang-format off
    auto outer = SC_MATCH_R (ConvExp<ObjectTypeConversion>, from, to) {
        [&](PointerType const&, PointerType const&) { return ConvNoop; },
        [&](UniquePtrType const&, RawPtrType const&) {
            using enum ConversionKind;
            return kind == Explicit ? ConvExp{ UniqueToRawPtr } : ConvError;
        },
        [&](RawPtrType const&, UniquePtrType const&) { return ConvError; }
    }; // clang-format on
    if (outer.isError()) {
        return std::nullopt;
    }
    if (outer.hasValue()) {
        result.push_back(outer.value());
    }
    if (isa<ArrayType>(*from.base()) || isa<ArrayType>(*to.base())) {
        auto inner = determineObjConv(kind, { from.base(), LValue },
                                      { to.base(), LValue });
        if (!inner) {
            return std::nullopt;
        }
        if (inner.has_value()) {
            auto conv = inner.value();
            auto asPtrConv = conv | transform(convRefToPtr);
            result.insert(result.end(), asPtrConv.begin(), asPtrConv.end());
        }
    }
    if (result.empty()) {
        return ConvChain{};
    }
    return result;
}

static ConvChain fromNullPointerConv(NullPtrType const&, RawPtrType const&) {
    return { ObjectTypeConversion::NullptrToRawPtr };
}

static ConvChain fromNullPointerConv(NullPtrType const&, UniquePtrType const&) {
    return { ObjectTypeConversion::NullptrToUniquePtr };
}

static std::optional<ConvChain> constructingConversion(ConversionKind kind,
                                                       ThinExpr from,
                                                       ThinExpr to) {
    if (to.isLValue()) {
        if (from.type().get() == to.type().get()) {
            return ConvChain{};
        }
        else {
            return std::nullopt;
        }
    }
    return computeObjectConstruction(kind, to.type().get(), std::array{ from })
        .visit(utl::overload{
            [](ObjectTypeConversion conv) {
        return std::optional(ConvChain{ conv });
    },
            [](ConvExp<ObjectTypeConversion>::Noop) {
        return std::optional(ConvChain{});
    },
            [](ConvExp<ObjectTypeConversion>::Error)
                -> std::optional<ConvChain> { return {}; },
        });
};

///
static bool alwaysWantConstructingConversion(ConversionKind kind, ThinExpr from,
                                             ThinExpr to) {
    /// Reinterpret casts are never object constructing
    if (kind == ConversionKind::Reinterpret) {
        return false;
    }
    if (isDynArray(*to.type()) && to.isRValue()) {
        return true;
    }
    if (to.type()->hasTrivialLifetime()) {
        return false;
    }
    return from.isLValue() && to.isRValue();
}

static std::optional<ConvChain> determineObjConv(ConversionKind kind,
                                                 ThinExpr from, ThinExpr to) {
    using enum ObjectTypeConversion;
    using RetType = std::optional<ConvChain>;
    ///
    if (alwaysWantConstructingConversion(kind, from, to)) {
        return constructingConversion(kind, from, to);
    }
    switch (kind) {
    case ConversionKind::Implicit:
        // clang-format off
        return SC_MATCH_R (RetType, *from.type(), *to.type()) {
            [&](IntType const&, ByteType const&) {
                return std::nullopt;
            },
            [&](ByteType const&, IntType const&) {
                return std::nullopt;
            },
            [&](IntType const& from, IntType const& to) {
                return implExplIntConversion(ConversionKind::Implicit, from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() == to.bitwidth()) {
                    return ConvChain{};
                }
                else if (from.bitwidth() < to.bitwidth()) {
                    SC_ASSERT(from.bitwidth() == 32, "");
                    SC_ASSERT(to.bitwidth() == 64, "");
                    return ConvChain{ FloatWidenTo64 };
                }
                else {
                    return std::nullopt;
                }
            },
            [&](IntType const&, FloatType const&) {
                return std::nullopt;
            },
            [&](FloatType const&, IntType const&) {
                return std::nullopt;
            },
            [&](ArrayType const& from, ArrayType const& to) {
                return implAndExplArrayToArrayConv(from, to);
            },
            [&](std::derived_from<PointerType> auto const& from,
                std::derived_from<PointerType> auto const& to) {
                return pointerConv(ConversionKind::Implicit, from, to);
            },
            [&](NullPtrType const& from,
                std::derived_from<PointerType> auto const& to) {
                return fromNullPointerConv(from, to);
            },
            [&](ObjectType const&, ObjectType const&) {
                return constructingConversion(ConversionKind::Implicit, from, to);
            }
        }; // clang-format on
    case ConversionKind::Explicit:
        // clang-format off
        return SC_MATCH_R (RetType, *from.type(), *to.type()) {
            [&](IntType const& from, ByteType const&) {
                ConvChain result;
                if (from.isSigned()) {
                    result.push_back(SignedToUnsigned);
                }
                if (from.bitwidth() > 8) {
                    result.push_back(IntTruncTo8);
                }
                result.push_back(IntToByte);
                return result;
            },
            [&](ByteType const&, IntType const& to) {
                auto byteConv = to.isSigned() ? ByteToSigned : ByteToUnsigned;
                switch (to.bitwidth()) {
                case 8:
                    return ConvChain{ byteConv };
                case 16:
                    return ConvChain{ 
                        byteConv,
                        to.isSigned() ? SignedWidenTo16 : UnsignedWidenTo16
                    };
                case 32:
                    return ConvChain{ 
                        byteConv,
                        to.isSigned() ? SignedWidenTo32 : UnsignedWidenTo32
                    };
                case 64:
                    return ConvChain{ 
                        byteConv,
                        to.isSigned() ? SignedWidenTo64 : UnsignedWidenTo64
                    };
                default:
                    SC_UNREACHABLE();
                }
            },
            [&](IntType const& from, IntType const& to) {
                return implExplIntConversion(ConversionKind::Explicit, from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() == to.bitwidth()) {
                    return ConvChain{};
                }
                else if (from.bitwidth() < to.bitwidth()) {
                    return ConvChain{ FloatWidenTo64 };
                }
                else {
                    return ConvChain{ FloatTruncTo32 };
                }
            },
            [&](IntType const& from, FloatType const& to) {
                switch (to.bitwidth()) {
                case 32:
                    return ConvChain{
                        from.isSigned() ? SignedToFloat32 : UnsignedToFloat32
                    };
                case 64:
                    return ConvChain{
                        from.isSigned() ? SignedToFloat64 : UnsignedToFloat64
                    };
                default:
                    SC_UNREACHABLE();
                }
            },
            [&](FloatType const&, IntType const& to) {
                switch (to.bitwidth()) {
                case 8:
                    return ConvChain{ 
                        to.isSigned() ? FloatToSigned8 : FloatToUnsigned8
                    };
                case 16:
                    return ConvChain{ 
                        to.isSigned() ? FloatToSigned16 : FloatToUnsigned16
                    };
                case 32:
                    return ConvChain{ 
                        to.isSigned() ? FloatToSigned32 : FloatToUnsigned32
                    };
                case 64:
                    return ConvChain{ 
                        to.isSigned() ? FloatToSigned64 : FloatToUnsigned64
                    };
                default:
                    SC_UNREACHABLE();
                }
            },
            [&](ArrayType const& from, ArrayType const& to) {
                return implAndExplArrayToArrayConv(from, to);
            },
            [&](std::derived_from<PointerType> auto const& from,
                std::derived_from<PointerType> auto const& to) {
                return pointerConv(ConversionKind::Explicit, from, to);
            },
            [&](NullPtrType const& from,
                std::derived_from<PointerType> auto const& to) {
                return fromNullPointerConv(from, to);
            },
            [&](ObjectType const&, ObjectType const&) {
                return constructingConversion(ConversionKind::Explicit, from, to);
            }
        }; // clang-format on
    case ConversionKind::Reinterpret:
        if (!to.type()->hasTrivialLifetime()) {
            return std::nullopt;
        }
        if (to.isRValue() && !isa<PointerType>(*to.type())) {
            if (from.type()->size() != to.type()->size()) {
                return std::nullopt;
            }
            return ConvChain{ Reinterpret_Value };
        }
        // clang-format off
        return SC_MATCH_R (RetType, *from.type(), *to.type()) {
            [&](IntType const& from, ByteType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return ConvChain{ Reinterpret_ValueRef };
                }
                return ConvChain{};
            },
            [&](ByteType const& from, IntType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return std::nullopt;
                }
                return ConvChain{ Reinterpret_ValueRef };
            },
            [&](IntType const& from, IntType const& to) -> RetType {
                if (&from == &to) {
                    return ConvChain{};
                }
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return ConvChain{ Reinterpret_ValueRef };
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (&from == &to) {
                    return ConvChain{};
                }
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return ConvChain{};
            },
            [&](IntType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return ConvChain{ Reinterpret_ValueRef };
            },
            [&](FloatType const& from, IntType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return ConvChain{ Reinterpret_ValueRef };
            },
            [&](ArrayType const& from, ObjectType const& to) -> RetType {
                auto* fromElem = from.elementType();
                if (!isa<ByteType>(fromElem)) {
                    return std::nullopt;
                }
                if (!from.isDynamic() && from.count() != to.size()) {
                    return std::nullopt;
                }
                return ConvChain{ Reinterpret_ValueRef_FromByteArray };
            },
            [&](ObjectType const& from, ArrayType const& to) -> RetType {
                auto* toElem = to.elementType();
                if (!isa<ByteType>(toElem)) {
                    return std::nullopt;
                }
                if (to.isStatic()) {
                    if (to.count() != from.size()) {
                        return std::nullopt;
                    }
                    return ConvChain{ Reinterpret_ValueRef_ToByteArray };
                }
                return ConvChain{
                    Reinterpret_ValueRef_ToByteArray, ArrayRef_FixedToDynamic
                };
            },
            [&](ArrayType const& from, ArrayType const& to) -> RetType {
                if (&from == &to) {
                    return ConvChain{};
                }
                if (to.isStatic()) {
                    if (from.isDynamic()) {
                        return std::nullopt;
                    }
                    if (from.size() != to.size()) {
                        return std::nullopt;
                    }
                }
                ConvChain result;
                if (from.isStatic() && to.isDynamic()) {
                    result.push_back(ArrayRef_FixedToDynamic);
                }
                return SC_MATCH_R (RetType,
                                   *from.elementType(),
                                   *to.elementType()) {
                    [&](ByteType const&, ByteType const&) {
                        return result;
                    },
                    [&](ByteType const&, ObjectType const&) {
                        result.push_back(
                            to.isStatic() ? Reinterpret_ValueRef :
                                            Reinterpret_DynArrayRef_FromByte);
                        return result;
                    },
                    [&](ObjectType const&, ByteType const&) {
                        result.push_back(
                            to.isStatic() ? Reinterpret_ValueRef :
                                            Reinterpret_DynArrayRef_ToByte);
                        return result;
                    },
                    [&](ObjectType const&, ObjectType const&) {
                        return std::nullopt;
                    }
                };
            },
            [&](std::derived_from<PointerType> auto const& from,
                std::derived_from<PointerType> auto const& to) {
                return pointerConv(ConversionKind::Reinterpret, from, to);
            },
            [&](NullPtrType const& from,
                std::derived_from<PointerType> auto const& to) {
                return fromNullPointerConv(from, to);
            },
            [&]([[maybe_unused]] ObjectType const& from,
                [[maybe_unused]] ObjectType const& to) -> RetType {
                if (&from == &to) {
                    return ConvChain{};
                }
                return std::nullopt;
            }
        }; // clang-format on
    }
    SC_UNREACHABLE();
}

static ConvExp<ValueCatConversion> determineValueCatConv(
    ConversionKind kind, ValueCategory from, ValueCategory to,
    Mutability toMutability) {
    using enum ConversionKind;
    using enum ValueCatConversion;
    using enum Mutability;

    if (from == to) {
        return ConvNoop;
    }
    if (from == LValue) {
        return LValueToRValue;
    }
    SC_ASSERT(from == RValue, "Other cases are checked above");
    switch (kind) {
    case Implicit:
        if (toMutability == Const) {
            return MaterializeTemporary;
        }
        else {
            return ConvError;
        }
    case Explicit:
        return MaterializeTemporary;
    case Reinterpret:
        return ConvError;
    }
    SC_UNREACHABLE();
}

static ConvExp<MutConversion> determineMutConv(ConversionKind, Mutability from,
                                               Mutability to) {
    /// No mutability conversion happens
    if (from == to) {
        return ConvNoop;
    }
    switch (from) {
    case Mutability::Mutable: // Mutable to Const:
        return MutConversion::MutToConst;
    case Mutability::Const: // Const to Mutable
        return ConvError;
    }
    SC_UNREACHABLE();
}

static int getRank(std::optional<ValueCatConversion> conv) {
    if (!conv) {
        return 0;
    }
    return std::array{
#define SC_VALUECATCONV_DEF(Name, Rank) Rank,
#include "Sema/Conversion.def"
    }[(size_t)conv.value()];
}

static int getRank(std::optional<MutConversion> conv) {
    if (!conv) {
        return 0;
    }
    return std::array{
#define SC_MUTCONV_DEF(Name, Rank) Rank,
#include "Sema/Conversion.def"
    }[(size_t)conv.value()];
}

static int getRank(std::optional<ObjectTypeConversion> conv) {
    if (!conv) {
        return 0;
    }
    return std::array{
#define SC_OBJTYPECONV_DEF(Name, Rank) Rank,
#include "Sema/Conversion.def"
    }[(size_t)conv.value()];
}

int sema::computeRank(Conversion const& conv) {
    return getRank(conv.valueCatConversion()) + getRank(conv.mutConversion()) +
           ranges::accumulate(conv.objectConversions(), 0, ranges::max,
                              [](auto conv) { return getRank(conv); });
}

/// Computes the greatest integer representable in a floating point value with
/// \p bitwidth bits of precision
static APInt computeIntegralFloatLimit(size_t fromBitwidth, size_t toBitwidth) {
    auto prec = toBitwidth == 32 ? APFloatPrec::Single() :
                                   APFloatPrec::Double();
    return lshl(APInt(1, fromBitwidth),
                utl::narrow_cast<int>(prec.mantissaWidth + 1));
}

static UniquePtr<Value> intTruncLossless(IntValue const* operand,
                                         size_t toBitwidth) {
    auto value = operand->value();
    if (operand->isSigned()) {
        if (scmp(value, sext(APInt::SMax(toBitwidth), value.bitwidth())) <= 0 &&
            scmp(value, sext(APInt::SMin(toBitwidth), value.bitwidth())) >= 0)
        {
            return allocate<IntValue>(zext(value, toBitwidth), true);
        }
        return nullptr;
    }
    else {
        if (ucmp(value, zext(APInt::UMax(toBitwidth), value.bitwidth())) <= 0 &&
            ucmp(value, zext(APInt::UMin(toBitwidth), value.bitwidth())) >= 0)
        {
            return allocate<IntValue>(zext(value, toBitwidth), true);
        }
        return nullptr;
    }
}

static bool fits(APFloat const& value, size_t bitwidth) {
    SC_ASSERT(value.precision() == APFloatPrec::Double(), "");
    SC_ASSERT(bitwidth == 32,
              "64 -> 32 is the only narrowing float conversion we have");
    if (std::isinf(value.to<float>())) {
        return false;
    }
    return true;
}

static UniquePtr<Value> floatTrunc(FloatValue const* operand,
                                   size_t targetBitwidth) {
    SC_EXPECT(targetBitwidth == 32);
    if (fits(operand->value(), targetBitwidth)) {
        return allocate<FloatValue>(
            APFloat(operand->value().to<float>(), APFloatPrec::Single()));
    }
    return nullptr;
}

static UniquePtr<Value> signedWiden(IntValue const* val, size_t toBitwith) {
    return allocate<IntValue>(sext(val->value(), toBitwith), val->isSigned());
}

static UniquePtr<Value> unsignedWiden(IntValue const* val, size_t toBitwith) {
    return allocate<IntValue>(zext(val->value(), toBitwith), val->isSigned());
}

static UniquePtr<Value> signedToFloat(IntValue const* val, size_t toBitwidth) {
    APInt limit = computeIntegralFloatLimit(val->bitwidth(), toBitwidth);
    if (scmp(val->value(), limit) <= 0 &&
        scmp(val->value(), negate(limit)) >= 0)
    {
        return allocate<FloatValue>(
            signedValuecast<APFloat>(val->value(), toBitwidth));
    }
    return nullptr;
}

static UniquePtr<Value> unsignedToFloat(IntValue const* val,
                                        size_t toBitwidth) {
    APInt limit = computeIntegralFloatLimit(val->bitwidth(), toBitwidth);
    if (ucmp(val->value(), limit) <= 0) {
        return allocate<FloatValue>(
            valuecast<APFloat>(val->value(), toBitwidth));
    }
    return nullptr;
}

static UniquePtr<Value> convertLossless(ObjectTypeConversion conv,
                                        Value const* value) {
    auto* intVal = dyncast<IntValue const*>(value);
    auto* floatVal = dyncast<FloatValue const*>(value);
    using enum ObjectTypeConversion;
    switch (conv) {
    case IntTruncTo8:
        return intTruncLossless(intVal, 8);
    case IntTruncTo16:
        return intTruncLossless(intVal, 16);
    case IntTruncTo32:
        return intTruncLossless(intVal, 32);
    case SignedWidenTo16:
        return signedWiden(intVal, 16);
    case SignedWidenTo32:
        return signedWiden(intVal, 32);
    case SignedWidenTo64:
        return signedWiden(intVal, 64);
    case UnsignedWidenTo16:
        return unsignedWiden(intVal, 16);
    case UnsignedWidenTo32:
        return unsignedWiden(intVal, 32);
    case UnsignedWidenTo64:
        return unsignedWiden(intVal, 64);
    case FloatTruncTo32:
        return floatTrunc(floatVal, 32);
    case FloatWidenTo64:
        return allocate<FloatValue>(
            precisionCast(floatVal->value(), APFloatPrec::Double()));
    case SignedToUnsigned:
        [[fallthrough]];
    case UnsignedToSigned:
        if (intVal->value().highbit()) {
            return nullptr;
        }
        return allocate<IntValue>(intVal->value(), !intVal->isSigned());
    case SignedToFloat32:
        return signedToFloat(intVal, 32);
    case SignedToFloat64:
        return signedToFloat(intVal, 64);
    case UnsignedToFloat32:
        return unsignedToFloat(intVal, 32);
    case UnsignedToFloat64:
        return unsignedToFloat(intVal, 64);
    case IntToByte:
        return intTruncLossless(intVal, 8);
    default:
        return nullptr;
    }
}

static bool isLossless(ConvChain const& chain, Value const* value) {
    auto converted = value->clone();
    for (auto conv: chain) {
        if (!(converted = convertLossless(conv, converted.get()))) {
            break;
        }
    }
    return !!converted;
}

static std::optional<ConvChain> tryImplicitConstConv(Value const* value,
                                                     ThinExpr from,
                                                     ThinExpr to) {
    /// We try an explicit conversion
    auto result = determineObjConv(ConversionKind::Explicit, from, to);
    if (!result || !isLossless(result.value(), value)) {
        return std::nullopt;
    }
    return result;
}

Expected<Conversion, std::unique_ptr<SemaIssue>> sema::computeConversion(
    ConversionKind kind, ast::Expression const* expr, QualType to,
    ValueCategory toValueCat) {
    auto valueCatConv = determineValueCatConv(kind, expr->valueCategory(),
                                              toValueCat, to.mutability());
    if (valueCatConv.isError()) {
        return std::make_unique<BadValueCatConv>(nullptr, expr, toValueCat);
    }
    ConvExp<MutConversion> mutConv =
        toValueCat == RValue ?
            ConvNoop :
            determineMutConv(kind, expr->type().mutability(), to.mutability());
    if (mutConv.isError()) {
        return std::make_unique<BadMutConv>(nullptr, expr, to.mutability());
    }
    auto objConv = determineObjConv(kind, expr, { to, toValueCat });
    /// If we can't find an implicit conversion and we have a constant value,
    /// we try to find an extended constant implicit conversion
    if (!objConv && kind == ConversionKind::Implicit && expr->constantValue()) {
        objConv = tryImplicitConstConv(expr->constantValue(), expr,
                                       { to, toValueCat });
    }
    if (!objConv) {
        return std::make_unique<BadTypeConv>(nullptr, expr, to.get());
    }
    /// We don't need (and don't want) lvalue to rvalue conversion if we
    /// construct an object
    if (valueCatConv == ValueCatConversion::LValueToRValue &&
        ranges::any_of(objConv.value(), sema::isConstruction))
    {
        valueCatConv = ConvNoop;
    }
    return Conversion(expr->type(), to, toOpt(valueCatConv), toOpt(mutConv),
                      objConv.value());
}

ast::Expression* sema::convert(ConversionKind kind, ast::Expression* expr,
                               QualType to, ValueCategory toValueCat,
                               CleanupStack& dtors, AnalysisContext& ctx) {
    auto convres = computeConversion(kind, expr, to, toValueCat);
    if (!convres) {
        ctx.issueHandler().push(std::move(convres).error());
        return nullptr;
    }
    return insertConversion(expr, convres.value(), dtors, ctx);
}

static std::optional<size_t> nextBitwidth(size_t width) {
    switch (width) {
    case 8:
        return 16;
    case 16:
        return 32;
    case 32:
        return 64;
    case 64:
        return std::nullopt;
    default:
        SC_UNREACHABLE();
    }
}

IntType const* commonTypeSignedUnsigned(SymbolTable& sym, IntType const* a,
                                        IntType const* b) {
    SC_EXPECT(a->isSigned());
    SC_EXPECT(b->isUnsigned());
    if (a->bitwidth() > b->bitwidth()) {
        return a;
    }
    if (auto width = nextBitwidth(b->bitwidth())) {
        return sym.intType(*width, Signedness::Signed);
    }
    return nullptr;
}

static Mutability commonMutability(Mutability a, Mutability b) {
    return a == Mutability::Mutable ? b : a;
}

static Mutability commonMutability(QualType a, QualType b) {
    return commonMutability(a.mutability(), b.mutability());
}

static RawPtrType const* commonPointer(SymbolTable& sym, QualType aBase,
                                       QualType bBase) {
    if (aBase == bBase) {
        return sym.pointer(aBase);
    }
    if (aBase.get() == bBase.get()) {
        return sym.pointer(
            QualType(aBase.get(), commonMutability(aBase, bBase)));
    }
    // clang-format off
    return SC_MATCH (*aBase, *bBase) {
        [&](ArrayType const& a, ArrayType const& b) -> RawPtrType const* {
            if (a.elementType() != b.elementType()) {
                return nullptr;
            }
            SC_ASSERT(a.count() != b.count(), "Case a == b handled earlier");
            auto* dynArray = sym.arrayType(a.elementType());
            auto mut = commonMutability(aBase.mutability(),
                                        bBase.mutability());
            return sym.pointer(QualType(dynArray, mut));
        },
        [&](ObjectType const&, ObjectType const&) {
            return nullptr;
        }
    }; // clang-format on
}

QualType sema::commonType(SymbolTable& sym, QualType a, QualType b) {
    auto commonMut = commonMutability(a.mutability(), b.mutability());
    if (a.get() == b.get()) {
        return QualType(a.get(), commonMut);
    }
    // clang-format off
    auto* commonObjType = SC_MATCH(*a, *b) {
        [&](IntType const& a, IntType const& b) {
            if (a.isSigned() && b.isUnsigned()) {
                return commonTypeSignedUnsigned(sym, &a, &b);
            }
            if (b.isSigned() && a.isUnsigned()) {
                return commonTypeSignedUnsigned(sym, &b, &a);
            }
            SC_ASSERT(a.signedness() == b.signedness(), "");
            return sym.intType(std::max(a.bitwidth(), b.bitwidth()),
                               a.signedness());
        },
        [&](ArrayType const& a, ArrayType const& b) -> ArrayType const* {
            if (a.elementType() != b.elementType()) {
                return nullptr;
            }
            if (a.count() == b.count()) {
                return &a;
            }
            return sym.arrayType(a.elementType());
        },
        [&](PointerType const& type, NullPtrType const&) {
            return &type;
        },
        [&](NullPtrType const&, PointerType const& type) {
            return &type;
        },
        [&](RawPtrType const& a, RawPtrType const& b) -> RawPtrType const* {
            return commonPointer(sym, a.base(), b.base());
        },
        [&](ObjectType const&, ObjectType const&) -> ObjectType const* {
            return nullptr;
        }
    }; // clang-format on
    return QualType(commonObjType, commonMut);
}

QualType sema::commonType(SymbolTable& sym, std::span<QualType const> types) {
    if (types.empty()) {
        return sym.Void();
    }
    QualType result = types[0];
    for (QualType type: types | ranges::views::drop(1)) {
        if (!result) {
            break;
        }
        result = commonType(sym, result, type);
    }
    return result;
}

QualType sema::commonType(SymbolTable& sym,
                          std::span<ast::Expression const* const> exprs) {
    return commonType(sym, exprs | ranges::views::transform([](auto* expr) {
        return expr->type();
    }) | ToSmallVector<>);
}

ConvExp<ObjectTypeConversion> sema::computeObjectConstruction(
    ConversionKind kind, ObjectType const* targetType,
    std::span<ThinExpr const> arguments) {
    SC_EXPECT(kind != ConversionKind::Reinterpret);
    using enum ObjectTypeConversion;
    auto& md = targetType->lifetimeMetadata();
    /// Dynamic array types
    if (isDynArray(*targetType)) {
        return DynArrayConstruct;
    }
    /// Default construction
    if (arguments.empty()) {
        using enum LifetimeOperation::Kind;
        switch (md.defaultConstructor().kind()) {
        case Deleted:
            return ConvError;
        case Trivial:
            return TrivDefConstruct;
        case Nontrivial:
            return NontrivConstruct;
        case NontrivialInline:
            return NontrivInlineConstruct;
        }
    }
    /// Copy construction
    if (arguments.size() == 1 && arguments.front().type().get() == targetType) {

        if (arguments.front().isRValue()) {
            return ConvNoop;
        }

        using enum LifetimeOperation::Kind;
        switch (md.copyConstructor().kind()) {
        case Deleted:
            return ConvError;
        case Trivial:
            return TrivCopyConstruct;
        case Nontrivial:
            return NontrivConstruct;
        case NontrivialInline:
            return NontrivInlineConstruct;
        }
    }
    /// The following conversions are explicit
    if (kind != ConversionKind::Explicit) {
        return ConvError;
    }
    /// Aggregate construction
    if (isAggregate(targetType)) {
        if (targetType->hasTrivialLifetime()) {
            return TrivAggrConstruct;
        }
        else {
            return NontrivAggrConstruct;
        }
    }
    /// Nontrivial object type construction
    if (isa<StructType>(targetType)) {
        return NontrivConstruct;
    }
    else {
        return NontrivInlineConstruct;
    }
}

UniquePtr<ast::ConstructBase> sema::allocateObjectConstruction(
    ObjectTypeConversion kind, SourceRange sourceRng,
    ObjectType const* targetType,
    utl::small_vector<UniquePtr<ast::Expression>> arguments) {
    using enum ObjectTypeConversion;
    switch (kind) {
    case TrivDefConstruct:
        SC_EXPECT(arguments.empty());
        return allocate<ast::TrivDefConstructExpr>(sourceRng, targetType);
    case TrivCopyConstruct:
        SC_EXPECT(arguments.size() == 1);
        return allocate<ast::TrivCopyConstructExpr>(std::move(
                                                        arguments.front()),
                                                    sourceRng, targetType);
    case TrivAggrConstruct:
        return allocate<ast::TrivAggrConstructExpr>(std::move(arguments),
                                                    sourceRng, targetType);
    case NontrivAggrConstruct:
        return allocate<ast::NontrivAggrConstructExpr>(std::move(arguments),
                                                       sourceRng,
                                                       cast<StructType const*>(
                                                           targetType));
    case NontrivConstruct:
        return allocate<ast::NontrivConstructExpr>(std::move(arguments),
                                                   sourceRng,
                                                   cast<StructType const*>(
                                                       targetType));
    case NontrivInlineConstruct:
        return allocate<ast::NontrivInlineConstructExpr>(std::move(arguments),
                                                         sourceRng, targetType);
    case DynArrayConstruct:
        return allocate<ast::DynArrayConstructExpr>(std::move(arguments),
                                                    sourceRng,
                                                    cast<ArrayType const*>(
                                                        targetType));
    default:
        SC_UNREACHABLE();
    }
}

ast::Expression* sema::constructInplace(
    ConversionKind kind, ast::Expression* replace, ObjectType const* targetType,
    std::span<ast::Expression* const> arguments, CleanupStack& cleanups,
    AnalysisContext& ctx) {
    auto insert =
        [parent = replace->parent(),
         index = replace->indexInParent()](UniquePtr<ast::Expression> expr) {
        return parent->setChild(index, std::move(expr));
    };
    auto result = constructInplace(kind, replace, insert, targetType, arguments,
                                   cleanups, ctx);
    switch (result.state()) {
    case ConvNoop:
        return replace;
    case ConvError:
        return nullptr;
    case ConvSuccess:
        return result.value();
    }
}

ConvExp<ast::Expression*> sema::constructInplace(
    ConversionKind kind, ast::ASTNode const* parentNode,
    utl::function_view<ast::Expression*(UniquePtr<ast::Expression>)> insert,
    ObjectType const* targetType, std::span<ast::Expression* const> arguments,
    CleanupStack& cleanups, AnalysisContext& ctx) {
    auto constrKind =
        computeObjectConstruction(kind, targetType,
                                  arguments |
                                      ranges::to<utl::small_vector<ThinExpr>>);
    if (constrKind.isNoop()) {
        return ConvNoop;
    }
    if (constrKind.isError()) {
        ctx.badExpr(parentNode, BadExpr::CannotConstructType);
        return ConvError;
    }
    auto constr = allocateObjectConstruction(
        constrKind.value(), parentNode->sourceRange(), targetType,
        arguments | transform(&ast::Expression::extractFromParent) |
            ToSmallVector<>);
    return analyzeValueExpr(insert(std::move(constr)), cleanups, ctx);
}

ast::Expression* sema::insertConversion(ast::Expression* expr, Conversion conv,
                                        CleanupStack& dtors,
                                        AnalysisContext& ctx) {
    if (auto mutConv = conv.mutConversion()) {
        expr = ast::insertNode<ast::MutConvExpr>(expr, *mutConv);
    }
    if (auto valueCatConv = conv.valueCatConversion()) {
        expr = ast::insertNode<ast::ValueCatConvExpr>(expr, *valueCatConv);
    }
    for (auto [index, objConv]: conv.objectConversions() | enumerate) {
        expr = ast::insertNode(expr,
                               [&](UniquePtr<ast::Expression> expr)
                                   -> UniquePtr<ast::Expression> {
            if (isConstruction(objConv)) {
                SC_ASSERT(conv.objectConversions().size() == 1, "");
                return allocateObjectConstruction(objConv, expr->sourceRange(),
                                                  conv.targetType().get(),
                                                  toSmallVector(
                                                      std::move(expr)));
            }
            else {
                auto* type = index == conv.objectConversions().size() - 1 ?
                                 conv.targetType().get() :
                                 nullptr;
                return allocate<ast::ObjTypeConvExpr>(std::move(expr), objConv,
                                                      type);
            }
        });
    }
    return analyzeValueExpr(expr, dtors, ctx);
}
