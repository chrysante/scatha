#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>
#include <variant>

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

/// \Pre expects \p from and \p to to not be the same
static ConvExp<ObjectTypeConversion> implicitIntConversion(IntType const& from,
                                                           IntType const& to) {
    if (&from == &to) {
        return ConvNoop;
    }
    using enum ObjectTypeConversion;
    if (from.isSigned()) {
        if (to.isSigned()) {
            /// ** `Signed -> Signed` **
            if (from.bitwidth() <= to.bitwidth()) {
                return SS_Widen;
            }
            return ConvError;
        }
        /// ** `Signed -> Unsigned` **
        return ConvError;
    }
    if (to.isSigned()) {
        /// ** `Unsigned -> Signed` **
        if (from.bitwidth() < to.bitwidth()) {
            return US_Widen;
        }
        return ConvError;
    }
    /// ** `Unsigned -> Unsigned` **
    if (from.bitwidth() <= to.bitwidth()) {
        return UU_Widen;
    }
    return ConvError;
}

/// \Pre expects \p from and \p to to not be the same
static ConvExp<ObjectTypeConversion> explicitIntConversion(IntType const& from,
                                                           IntType const& to) {
    if (&from == &to) {
        return ConvNoop;
    }
    using enum ObjectTypeConversion;
    if (from.isSigned()) {
        if (to.isSigned()) {
            /// ** `Signed -> Signed` **
            if (from.bitwidth() <= to.bitwidth()) {
                return SS_Widen;
            }
            return SS_Trunc;
        }
        /// ** `Signed -> Unsigned` **
        if (from.bitwidth() <= to.bitwidth()) {
            return SU_Widen;
        }
        return SU_Trunc;
    }
    if (to.isSigned()) {
        /// ** `Unsigned -> Signed` **
        if (from.bitwidth() < to.bitwidth()) {
            return US_Widen;
        }
        return US_Trunc;
    }
    /// ** `Unsigned -> Unsigned` **
    if (from.bitwidth() < to.bitwidth()) {
        return SU_Widen;
    }
    return SU_Trunc;
}

static ConvExp<ObjectTypeConversion> implAndExplArrayToArrayConv(
    ArrayType const& from, ArrayType const& to) {
    using enum ObjectTypeConversion;
    if (&from == &to) {
        return ConvNoop;
    }
    if (from.elementType() == to.elementType() && !from.isDynamic() &&
        to.isDynamic())
    {
        return ArrayRef_FixedToDynamic;
    }
    return ConvError;
};

/// Forward declaration here because `pointerConv` uses this recursively
static ConvExp<ObjectTypeConversion> determineObjConv(ConversionKind kind,
                                                      ThinExpr from,
                                                      ThinExpr to);

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
    case Reinterpret_ArrayRef_ToByte:
        return Reinterpret_ArrayPtr_ToByte;
    case Reinterpret_ArrayRef_FromByte:
        return Reinterpret_ArrayPtr_FromByte;
    default:
        SC_UNREACHABLE();
    }
}

static ConvExp<ObjectTypeConversion> pointerConv(ConversionKind kind,
                                                 PointerType const& from,
                                                 PointerType const& to) {
    using R = ConvExp<ObjectTypeConversion>;
    using enum ObjectTypeConversion;
    if (&from == &to) {
        return ConvNoop;
    }
    if (from.base().isConst() && to.base().isMut()) {
        return ConvError;
    }
    if (from.base().get() == to.base().get()) {
        // clang-format off
        return SC_MATCH_R (R, from, to) {
            [](PointerType const&, PointerType const&) { return ConvNoop; },
            [](UniquePtrType const&, RawPtrType const&) {
                return UniqueToRawPtr;
            },
            [](RawPtrType const&, UniquePtrType const&) { return ConvError; }
        }; // clang-format on
    }
    if (isa<ArrayType>(*from.base()) || isa<ArrayType>(*to.base())) {
        auto conv = determineObjConv(kind, { from.base(), LValue },
                                     { to.base(), LValue });
        return conv.transform(convRefToPtr);
    }
    return ConvError;
}

static ObjectTypeConversion fromNullPointerConv(NullPtrType const&,
                                                RawPtrType const&) {
    return ObjectTypeConversion::NullptrToRawPtr;
}

static ObjectTypeConversion fromNullPointerConv(NullPtrType const&,
                                                UniquePtrType const&) {
    return ObjectTypeConversion::NullptrToUniquePtr;
}

static ConvExp<ObjectTypeConversion> constructingConversion(ConversionKind kind,
                                                            ThinExpr from,
                                                            ThinExpr to) {
    if (to.isLValue()) {
        if (from.type().get() == to.type().get()) {
            return ConvNoop;
        }
        else {
            return ConvError;
        }
    }
    return computeObjectConstruction(kind, to.type().get(), std::array{ from });
};

///
static bool wantConstructingConversion(ThinExpr from, ThinExpr to) {
    return !to.type()->hasTrivialLifetime() && from.isLValue() && to.isRValue();
}

static ConvExp<ObjectTypeConversion> determineObjConv(ConversionKind kind,
                                                      ThinExpr from,
                                                      ThinExpr to) {
    using enum ObjectTypeConversion;
    using RetType = ConvExp<ObjectTypeConversion>;
    ///
    if (wantConstructingConversion(from, to)) {
        return constructingConversion(kind, from, to);
    }
    switch (kind) {
    case ConversionKind::Implicit:
        // clang-format off
        return SC_MATCH_R (RetType, *from.type(), *to.type()) {
            [&](IntType const&, ByteType const&) {
                return ConvError;
            },
            [&](ByteType const&, IntType const&) {
                return ConvError;
            },
            [&](IntType const& from, IntType const& to) {
                return implicitIntConversion(from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (&from == &to) {
                    return ConvNoop;
                }
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return ConvError;
            },
            [&](IntType const&, FloatType const&) {
                return ConvError;
            },
            [&](FloatType const&, IntType const&) {
                return ConvError;
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
            [&](IntType const& from, ByteType const& to) {
                if (from.isSigned()) {
                    return from.size() == to.size() ? SU_Widen : SU_Trunc;
                }
                return from.size() == to.size() ? UU_Widen : UU_Trunc;
            },
            [&]([[maybe_unused]] ByteType const& from,
                [[maybe_unused]] IntType const& to) {
                if (to.isSigned()) {
                    return US_Widen;
                }
                return UU_Widen;
            },
            [&](IntType const& from, IntType const& to) {
                return explicitIntConversion(from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (&from == &to) {
                    return ConvNoop;
                }
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return Float_Trunc;
            },
            [&](IntType const& from, FloatType const&) {
                return from.isSigned() ? SignedToFloat : UnsignedToFloat;
            },
            [&](FloatType const&, IntType const& to) {
                return to.isSigned() ? FloatToSigned : FloatToUnsigned;
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
        // clang-format off
        return SC_MATCH_R (RetType, *from.type(), *to.type()) {
            [&](IntType const& from, ByteType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return Reinterpret_ValueRef;
                }
                return ConvNoop;
            },
            [&](ByteType const& from, IntType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef;
            },
            [&](IntType const& from, IntType const& to) -> RetType {
                if (&from == &to) {
                    return ConvNoop;
                }
                if (from.bitwidth() != to.bitwidth()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef;
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (&from == &to) {
                    return ConvNoop;
                }
                if (from.bitwidth() != to.bitwidth()) {
                    return ConvError;
                }
                return ConvNoop;
            },
            [&](IntType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef;
            },
            [&](FloatType const& from, IntType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef;
            },
            [&](ArrayType const& from, ObjectType const& to) -> RetType {
                auto* fromElem = from.elementType();
                if (!isa<ByteType>(fromElem)) {
                    return ConvError;
                }
                if (!from.isDynamic() && from.count() != to.size()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef_FromByteArray;
            },
            [&](ObjectType const& from, ArrayType const& to) -> RetType {
                auto* toElem = to.elementType();
                if (!isa<ByteType>(toElem)) {
                    return ConvError;
                }
                if (!to.isDynamic() && to.count() != from.size()) {
                    return ConvError;
                }
                return Reinterpret_ValueRef_ToByteArray;
            },
            [&](ArrayType const& from, ArrayType const& to) -> RetType {
                if (&from == &to) {
                    return ConvNoop;
                }
                if (!to.isDynamic() && from.isDynamic()) {
                    return ConvError;
                }
                if (!to.isDynamic() && from.size() != to.size()) {
                    return ConvError;
                }
                return SC_MATCH_R (RetType,
                                   *from.elementType(),
                                   *to.elementType()) {
                    [](ByteType const&, ByteType const&) {
                        return ConvNoop;
                    },
                    [](ByteType const&, ObjectType const&) {
                        return Reinterpret_ArrayRef_FromByte;
                    },
                    [](ObjectType const&, ByteType const&) {
                        return Reinterpret_ArrayRef_ToByte;
                    },
                    [](ObjectType const&, ObjectType const&) {
                        return ConvError;
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
                    return ConvNoop;
                }
                return ConvError;
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
           getRank(conv.objectConversion());
}

static bool fits(APInt const& value, size_t numDestBits, bool destIsSigned) {
    if (value.bitwidth() <= numDestBits) {
        return true;
    }
    auto ref = zext(value, numDestBits);
    if (destIsSigned) {
        ref.sext(value.bitwidth());
    }
    else {
        ref.zext(value.bitwidth());
    }
    return value == ref;
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

/// Computes the greatest integer representable in a floating point value with
/// \p bitwidth bits of precision
static APInt computeIntegralFloatLimit(size_t fromBitwidth, size_t toBitwidth) {
    auto prec = toBitwidth == 32 ? APFloatPrec::Single() :
                                   APFloatPrec::Double();
    return lshl(APInt(1, fromBitwidth),
                utl::narrow_cast<int>(prec.mantissaWidth + 1));
}

static bool isLossless(std::optional<ObjectTypeConversion> conv,
                       Value const* value, ObjectType const* from,
                       ObjectType const* to) {
    /// TODO: Introduce `SignedToUnsigned` and vv. conversion
    /// Unfortunately `ConvNoop` also represents signed -> unsigned and vv.
    /// conversion (because they are noops in hardware)
    if (!conv) {
        auto* iFrom = dyncast<IntType const*>(from);
        auto* iTo = dyncast<IntType const*>(to);
        if (iFrom && iTo) {
            /// If we are changing signedness, make sure the value does not
            /// change This is equivalent to the high bit being == 0
            if (iFrom->signedness() != iTo->signedness()) {
                return cast<IntValue const*>(value)->value().highbit() == 0;
            }
            return true;
        }
        /// Float to float conversions come later
        return false;
    }
    using enum ObjectTypeConversion;
    switch (*conv) {
    case SS_Trunc:
        [[fallthrough]];
    case SU_Trunc:
        [[fallthrough]];
    case US_Trunc:
        [[fallthrough]];
    case UU_Trunc:
        return fits(cast<IntValue const*>(value)->value(),
                    cast<ArithmeticType const*>(to)->bitwidth(),
                    cast<ArithmeticType const*>(to)->isSigned());
    case SS_Widen:
        return true;
    case SU_Widen:
        [[fallthrough]];
    case US_Widen: {
        auto intVal = cast<IntValue const*>(value)->value();
        return !intVal.negative();
    }
    case UU_Widen:
        return true;

    case Float_Trunc:
        return fits(cast<FloatValue const*>(value)->value(),
                    cast<FloatType const*>(to)->bitwidth());

    case Float_Widen:
        return true;

    case SignedToFloat: {
        auto* fromInt = cast<IntType const*>(from);
        auto* toFloat = cast<FloatType const*>(to);
        auto val = cast<IntValue const*>(value);
        APInt limit =
            computeIntegralFloatLimit(fromInt->bitwidth(), toFloat->bitwidth());
        if (scmp(val->value(), limit) <= 0) {
            return true;
        }
        if (scmp(val->value(), negate(limit)) >= 0) {
            return true;
        }
        return false;
    }
    case UnsignedToFloat: {
        auto* fromInt = cast<IntType const*>(from);
        auto* toFloat = cast<FloatType const*>(to);
        auto val = cast<IntValue const*>(value);
        APInt limit =
            computeIntegralFloatLimit(fromInt->bitwidth(), toFloat->bitwidth());
        if (ucmp(val->value(), limit) <= 0) {
            return true;
        }
        return false;
    }
    case FloatToSigned:
        [[fallthrough]];
    case FloatToUnsigned:
        return false;
    default:
        return false;
    }
}

static ConvExp<ObjectTypeConversion> tryImplicitConstConv(Value const* value,
                                                          ThinExpr from,
                                                          ThinExpr to) {
    /// We try an explicit conversion
    auto result = determineObjConv(ConversionKind::Explicit, from, to);
    if (result.isError() ||
        !isLossless(toOpt(result), value, from.type().get(), to.type().get()))
    {
        return ConvError;
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
    if (kind == ConversionKind::Implicit && objConv.isError() &&
        expr->constantValue())
    {
        objConv = tryImplicitConstConv(expr->constantValue(), expr,
                                       { to, toValueCat });
    }
    if (objConv.isError()) {
        return std::make_unique<BadTypeConv>(nullptr, expr, to.get());
    }
    return Conversion(expr->type(), to, toOpt(valueCatConv), toOpt(mutConv),
                      toOpt(objConv));
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
    auto mutConv = conv.mutConversion();
    auto vcConv = conv.valueCatConversion();
    auto objConv = conv.objectConversion();
    if (mutConv) {
        expr = ast::insertNode<ast::MutConvExpr>(expr, *mutConv);
    }
    if (vcConv) {
        if (*vcConv != ValueCatConversion::LValueToRValue || !objConv ||
            !isConstruction(*objConv))
        {
            expr = ast::insertNode<ast::ValueCatConvExpr>(expr, *vcConv);
        }
    }
    if (objConv) {
        if (isConstruction(*objConv)) {
            expr = ast::insertNode(expr, [&](UniquePtr<ast::Expression> expr) {
                return allocateObjectConstruction(*objConv, expr->sourceRange(),
                                                  conv.targetType().get(),
                                                  toSmallVector(
                                                      std::move(expr)));
            });
        }
        else {
            expr =
                ast::insertNode<ast::ObjTypeConvExpr>(expr, *objConv,
                                                      conv.targetType().get());
        }
    }
    return analyzeValueExpr(expr, dtors, ctx);
}
