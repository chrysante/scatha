#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;

std::string_view sema::toString(ValueCatConversion conv) {
    return std::array{
#define SC_VALUECATCONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream, ValueCatConversion conv) {
    return ostream << toString(conv);
}

std::string_view sema::toString(MutConversion conv) {
    return std::array{
#define SC_MUTCONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream, MutConversion conv) {
    return ostream << toString(conv);
}

std::string_view sema::toString(ObjectTypeConversion conv) {
    return std::array{
#define SC_OBJTYPECONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream,
                               ObjectTypeConversion conv) {
    return ostream << toString(conv);
}

Conversion::operator bool() const {
    return valueCatConversion() != ValueCatConversion::None ||
           mutConversion() != MutConversion::None ||
           objectConversion() != ObjectTypeConversion::None ||
           isObjectConstruction();
}

static std::optional<ObjectTypeConversion> implicitIntConversion(
    IntType const& from, IntType const& to) {
    using enum ObjectTypeConversion;
    if (from.isSigned()) {
        if (to.isSigned()) {
            /// ** `Signed -> Signed` **
            if (from.bitwidth() <= to.bitwidth()) {
                return SS_Widen;
            }
            return std::nullopt;
        }
        /// ** `Signed -> Unsigned` **
        return std::nullopt;
    }
    if (to.isSigned()) {
        /// ** `Unsigned -> Signed` **
        if (from.bitwidth() < to.bitwidth()) {
            return US_Widen;
        }
        return std::nullopt;
    }
    /// ** `Unsigned -> Unsigned` **
    if (from.bitwidth() <= to.bitwidth()) {
        return UU_Widen;
    }
    return std::nullopt;
}

static std::optional<ObjectTypeConversion> explicitIntConversion(
    IntType const& from, IntType const& to) {
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

static std::optional<ObjectTypeConversion> determineObjConv(
    ConversionKind kind, ObjectType const* from, ObjectType const* to) {
    using enum ObjectTypeConversion;
    if (from == to) {
        return None;
    }
    using RetType = std::optional<ObjectTypeConversion>;
    auto implAndExplArrayToArrayConv = [](ArrayType const& from,
                                          ArrayType const& to) -> RetType {
        if (from.elementType() == to.elementType() && !from.isDynamic() &&
            to.isDynamic())
        {
            return Array_FixedToDynamic;
        }
        return std::nullopt;
    };
    auto pointerConv = utl::overload{
        [&](NullPtrType const& from, PointerType const& to) -> RetType {
            return None;
        },
        [&](NullPtrType const& from, UniquePtrType const& to) -> RetType {
            return None;
        },
        [&](PointerType const& from, PointerType const& to) -> RetType {
            if (from.base().isConst() && to.base().isMut()) {
                return std::nullopt;
            }
            if (from.base().get() == to.base().get()) {
                return None;
            }
            auto* fromArray = dyncast<ArrayType const*>(from.base().get());
            auto* toArray = dyncast<ArrayType const*>(to.base().get());
            if (fromArray || toArray) {
                return determineObjConv(kind,
                                        from.base().get(),
                                        to.base().get());
            }
            return std::nullopt;
        },
        /// Yes, we allow implicit conversion from `*unique T` to `*T`
        [&](UniquePtrType const& from, PointerType const& to) -> RetType {
            if (from.base().isConst() && to.base().isMut()) {
                return std::nullopt;
            }
            if (from.base().get() == to.base().get()) {
                return None;
            }
            return std::nullopt;
        },
    };

    switch (kind) {
    case ConversionKind::Implicit:
        // clang-format off
        return SC_MATCH (*from, *to) {
            [&](IntType const& from, ByteType const& to) -> RetType {
                return std::nullopt;
            },
            [&](ByteType const& from, IntType const& to) -> RetType {
                return std::nullopt;
            },
            [&](IntType const& from, IntType const& to) -> RetType {
                return implicitIntConversion(from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return std::nullopt;
            },
            [&](IntType const& from, FloatType const& to) -> RetType {
                return std::nullopt;
            },
            [&](FloatType const& from, IntType const& to) -> RetType {
                return std::nullopt;
            },
            implAndExplArrayToArrayConv,
            pointerConv,
            [&](ObjectType const& from, ObjectType const& to) -> RetType {
                return std::nullopt;
            }
        }; // clang-format on
    case ConversionKind::Explicit:
        // clang-format off
        return SC_MATCH (*from, *to) {
            [&](IntType const& from, ByteType const& to) -> RetType {
                if (from.isSigned()) {
                    return from.size() == to.size() ? SU_Widen : SU_Trunc;
                }
                return from.size() == to.size() ? UU_Widen : UU_Trunc;
            },
            [&](ByteType const& from, IntType const& to) -> RetType {
                if (to.isSigned()) {
                    return US_Widen;
                }
                return UU_Widen;
            },
            [&](IntType const& from, IntType const& to) -> RetType {
                return explicitIntConversion(from, to);
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return Float_Trunc;
            },
            [&](IntType const& from, FloatType const& to) -> RetType {
                return from.isSigned() ? SignedToFloat : UnsignedToFloat;
            },
            [&](FloatType const& from, IntType const& to) -> RetType {
                return to.isSigned() ? FloatToSigned : FloatToUnsigned;
            },
            implAndExplArrayToArrayConv,
            pointerConv,
            [&](ObjectType const& from, ObjectType const& to) -> RetType {
                return std::nullopt;
            }
        }; // clang-format on
    case ConversionKind::Reinterpret:
        // clang-format off
        return SC_MATCH (*from, *to) {
            [&](IntType const& from, ByteType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return Reinterpret_Value;
                }
                return None;
            },
            [&](ByteType const& from, IntType const& to) -> RetType {
                if (from.size() != to.size()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            },
            [&](IntType const& from, IntType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            },
            [&](FloatType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return None;
            },
            [&](IntType const& from, FloatType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            },
            [&](FloatType const& from, IntType const& to) -> RetType {
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            },
            [&](ArrayType const& from, ObjectType const& to) -> RetType {
                auto* fromElem = from.elementType();
                if (!isa<ByteType>(fromElem)) {
                    return std::nullopt;
                }
                if (!from.isDynamic() && from.count() != to.size()) {
                    return std::nullopt;
                }
                return Reinterpret_Value_FromByteArray;
            },
            [&](ObjectType const& from, ArrayType const& to) -> RetType {
                auto* toElem = to.elementType();
                if (!isa<ByteType>(toElem)) {
                    return std::nullopt;
                }
                if (!to.isDynamic() && to.count() != from.size()) {
                    return std::nullopt;
                }
                return Reinterpret_Value_ToByteArray;
            },
            [&](ArrayType const& from, ArrayType const& to) -> RetType {
                if (!to.isDynamic() && from.isDynamic()) {
                    return std::nullopt;
                }
                if (!to.isDynamic() && from.size() != to.size()) {
                    return std::nullopt;
                }
                return visit(*from.elementType(), *to.elementType(), utl::overload{
                    [](ByteType const&, ByteType const&) -> RetType {
                        return None;
                    },
                    [](ByteType const&, ObjectType const&) -> RetType {
                        return Reinterpret_Array_FromByte;
                    },
                    [](ObjectType const&, ByteType const&) -> RetType {
                        return Reinterpret_Array_ToByte;
                    },
                    [](ObjectType const&, ObjectType const&) -> RetType {
                        return std::nullopt;
                    }
                });
            },
            pointerConv,
            [&](ObjectType const& from, ObjectType const& to) -> RetType {
                return std::nullopt;
            }
        }; // clang-format on
    }
}

static std::optional<ValueCatConversion> determineValueCatConv(
    ConversionKind kind,
    ValueCategory from,
    ValueCategory to,
    Mutability toMutability) {
    using enum ConversionKind;
    using enum ValueCatConversion;
    using enum Mutability;

    if (from == to) {
        return None;
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
            return std::nullopt;
        }
    case Explicit:
        return MaterializeTemporary;
    case Reinterpret:
        return std::nullopt;
    }
}

static std::optional<MutConversion> determineMutConv(ConversionKind kind,
                                                     Mutability from,
                                                     Mutability to) {
    /// No mutability conversion happens
    if (from == to) {
        return MutConversion::None;
    }
    switch (from) {
    case Mutability::Mutable: // Mutable to Const:
        return MutConversion::MutToConst;
    case Mutability::Const:   // Const to Mutable
        return std::nullopt;
    }
}

static int getRank(ValueCatConversion conv) {
    return std::array{
#define SC_VALUECATCONV_DEF(Name, Rank) Rank,
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

static int getRank(MutConversion conv) {
    return std::array{
#define SC_MUTCONV_DEF(Name, Rank) Rank,
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

static int getRank(ObjectTypeConversion conv) {
    return std::array{
#define SC_OBJTYPECONV_DEF(Name, Rank) Rank,
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
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
    SC_ASSERT(value.precision() == APFloatPrec::Double, "");
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
    auto prec = toBitwidth == 32 ? APFloatPrec::Single : APFloatPrec::Double;
    return lshl(APInt(1, fromBitwidth),
                utl::narrow_cast<int>(prec.mantissaWidth + 1));
}

static std::optional<ObjectTypeConversion> tryImplicitConstConv(
    Value const* value, ObjectType const* from, ObjectType const* to) {
    using enum ObjectTypeConversion;
    /// We try an explicit conversion
    auto result = determineObjConv(ConversionKind::Explicit, from, to);
    if (!result) {
        return std::nullopt;
    }
    /// If the explicit conversion succeeds, we check if we lose precision
    bool const lossless = [&] {
        switch (*result) {
        case None: {
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
            APInt limit = computeIntegralFloatLimit(fromInt->bitwidth(),
                                                    toFloat->bitwidth());
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
            APInt limit = computeIntegralFloatLimit(fromInt->bitwidth(),
                                                    toFloat->bitwidth());
            if (ucmp(val->value(), limit) <= 0) {
                return true;
            }
        }
        case FloatToSigned:
            [[fallthrough]];
        case FloatToUnsigned:
            return false;
        default:
            return false;
        }
    }();
    if (lossless) {
        return result;
    }
    return std::nullopt;
}

Expected<Conversion, std::unique_ptr<SemaIssue>> sema::computeConversion(
    ConversionKind kind,
    ast::Expression const* expr,
    QualType to,
    ValueCategory toValueCat) {
    auto valueCatConv = determineValueCatConv(kind,
                                              expr->valueCategory(),
                                              toValueCat,
                                              to.mutability());
    if (!valueCatConv) {
        return std::make_unique<BadValueCatConv>(nullptr, expr, toValueCat);
    }
    std::optional mutConv = MutConversion::None;
    if (toValueCat != RValue) {
        mutConv =
            determineMutConv(kind, expr->type().mutability(), to.mutability());
    }
    if (!mutConv) {
        return std::make_unique<BadMutConv>(nullptr, expr, to.mutability());
    }
    auto objConv = determineObjConv(kind, expr->type().get(), to.get());
    /// If we can't find an implicit conversion and we have a constant value,
    /// we try to find an extended constant implicit conversion
    if (kind == ConversionKind::Implicit && !objConv && expr->constantValue()) {
        objConv = tryImplicitConstConv(expr->constantValue(),
                                       expr->type().get(),
                                       to.get());
    }
    if (!objConv) {
        return std::make_unique<BadTypeConv>(nullptr, expr, to.get());
    }
    bool isObjConstr = false;
    if (*valueCatConv == ValueCatConversion::LValueToRValue) {
        *valueCatConv = ValueCatConversion::None;
        isObjConstr = true;
    }
    return Conversion(expr->type(),
                      to,
                      *valueCatConv,
                      *mutConv,
                      *objConv,
                      isObjConstr);
}

ast::Expression* sema::convert(ConversionKind kind,
                               ast::Expression* expr,
                               QualType to,
                               ValueCategory toValueCat,
                               DtorStack& dtors,
                               AnalysisContext& ctx) {
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

IntType const* commonTypeSignedUnsigned(SymbolTable& sym,
                                        IntType const* a,
                                        IntType const* b) {
    SC_ASSERT(a->isSigned(), "");
    SC_ASSERT(b->isUnsigned(), "");
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

static PointerType const* commonPointer(SymbolTable& sym,
                                        QualType aBase,
                                        QualType bBase) {
    SC_ASSERT(aBase != bBase, "");
    // clang-format off
    return SC_MATCH (*aBase, *bBase) {
        [&](ArrayType const& a, ArrayType const& b) -> PointerType const* {
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
            if (a.isDynamic()) {
                return &a;
            }
            if (b.isDynamic()) {
                return &b;
            }
            return nullptr;
        },
        [&](PointerType const& a, NullPtrType const& b) {
            return &a;
        },
        [&](NullPtrType const& a, PointerType const& b) {
            return &b;
        },
        [&](PointerType const& a, PointerType const& b) -> PointerType const* {
            if (a.base() == b.base()) {
                return &a;
            }
            return commonPointer(sym, a.base(), b.base());
        },
        [&](ObjectType const& a, ObjectType const& b) -> ObjectType const* {
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

ast::Expression* sema::insertConversion(ast::Expression* expr,
                                        Conversion conv,
                                        DtorStack& dtors,
                                        AnalysisContext& ctx) {
    if (!conv) {
        return expr;
    }
    expr = ast::insertNode<ast::Conversion>(expr,
                                            std::make_unique<Conversion>(conv));
    expr = analyzeValueExpr(expr, dtors, ctx);
    if (conv.isObjectConstruction()) {
        expr = ast::insertNode<ast::ConstructExpr>(expr);
        expr = analyzeValueExpr(expr, dtors, ctx);
    }
    return expr;
}
