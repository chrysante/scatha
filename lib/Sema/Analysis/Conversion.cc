#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Context.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(RefConversion conv) {
    return std::array{
#define SC_REFCONV_DEF(Name, ...) std::string_view(#Name),
#include "Sema/Analysis/Conversion.def"
    }[static_cast<size_t>(conv)];
}

std::ostream& sema::operator<<(std::ostream& ostream, RefConversion conv) {
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

bool Conversion::isNoop() const {
    return refConversion() == RefConversion::None &&
           mutConversion() == MutConversion::None &&
           objectConversion() == ObjectTypeConversion::None;
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
    // clang-format off
    return visit(*from, *to, utl::overload{
        [&](IntType const& from, ByteType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                return std::nullopt;
            case ConversionKind::Explicit:
                if (from.isSigned()) {
                    return from.size() == to.size() ? SU_Widen : SU_Trunc;
                }
                return from.size() == to.size() ? UU_Widen : UU_Trunc;
            case ConversionKind::Reinterpret:
                if (from.size() != to.size()) {
                    return Reinterpret_Value;
                }
                return None;
            }
        },
        [&](ByteType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                return std::nullopt;
            case ConversionKind::Explicit:
                if (to.isSigned()) {
                    return US_Widen;
                }
                return UU_Widen;
            case ConversionKind::Reinterpret:
                if (from.size() != to.size()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](IntType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                return implicitIntConversion(from, to);
            case ConversionKind::Explicit:
                return explicitIntConversion(from, to);
            case ConversionKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](FloatType const& from, FloatType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return std::nullopt;
            case ConversionKind::Explicit:
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return Float_Trunc;
            case ConversionKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return None;
            }
        },
        [&](IntType const& from, FloatType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                return std::nullopt;
            case ConversionKind::Explicit:
                return from.isSigned() ? SignedToFloat : UnsignedToFloat;
            case ConversionKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](FloatType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                return std::nullopt;
            case ConversionKind::Explicit:
                return to.isSigned() ? FloatToSigned : FloatToUnsigned;
            case ConversionKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](ArrayType const& from, ArrayType const& to) -> RetType {
            switch (kind) {
            case ConversionKind::Implicit:
                [[fallthrough]];
            case ConversionKind::Explicit:
                if (from.elementType() == to.elementType() &&
                    !from.isDynamic() && to.isDynamic())
                {
                    return Array_FixedToDynamic;
                }
                return std::nullopt;
            case ConversionKind::Reinterpret: {
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
            }
            }
        },
        [&](PointerType const& from, PointerType const& to) -> RetType {
            if (from.base().isConst() && to.base().isMutable()) {
                return std::nullopt;
            }
            if (from.base().get() == to.base().get()) {
                return None;
            }
            auto* fromArray = dyncast<ArrayType const*>(from.base().get());
            auto* toArray = dyncast<ArrayType const*>(to.base().get());
            if (fromArray && toArray) {
                return determineObjConv(kind, fromArray, toArray);
            }
            return std::nullopt;
        },
        [&](ObjectType const& from, ObjectType const& to) -> RetType {
            return std::nullopt;
        }
    }); // clang-format on
}

static std::optional<RefConversion> determineRefConv(ConversionKind kind,
                                                     QualType from,
                                                     QualType to) {
    using enum RefConversion;
    using Ret = std::optional<RefConversion>;
    // clang-format off
    return SC_MATCH (*from, *to) {
        [&](ObjectType const& from, ObjectType const& to) -> Ret {
            return None;
        },
        [&](ReferenceType const& from, ObjectType const& to) -> Ret {
            return Dereference;
        },
        [&](ObjectType const& from, ReferenceType const& to) -> Ret {
            if (kind == ConversionKind::Reinterpret) {
                return std::nullopt;
            }
            /// We can always bind to a const reference
            if (kind == ConversionKind::Explicit || to.base().isConst()) {
                return MaterializeTemporary;
            }
            return std::nullopt;
        },
        [&](ReferenceType const& from, ReferenceType const& to) -> Ret {
            return None;
        },
    }; // clang-format on
}

static std::optional<MutConversion> determineMutConv(ConversionKind kind,
                                                     QualType from,
                                                     QualType to) {
    /// Conversions to values are not concerned with mutability restrictions
    if (!isRef(to)) {
        return MutConversion::None;
    }
    auto fromBaseMut = stripReference(from).mutability();
    auto toBaseMut = stripReference(to).mutability();
    /// No mutability conversion happens
    if (fromBaseMut == toBaseMut) {
        return MutConversion::None;
    }
    switch (fromBaseMut) {
    case Mutability::Mutable: // Mutable to Const:
        return MutConversion::MutToConst;
    case Mutability::Const:   // Const to Mutable
        return std::nullopt;
    }
}

bool isCompatible(RefConversion refConv, ObjectTypeConversion objConv) {
    using enum ObjectTypeConversion;
    switch (refConv) {
    case RefConversion::None:
        return true;

    case RefConversion::Dereference:
        return true;

    case RefConversion::MaterializeTemporary:
        return true;
    }
}

static int getRank(RefConversion conv) {
    return std::array{
#define SC_REFCONV_DEF(Name, Rank) Rank,
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
    return getRank(conv.refConversion()) + getRank(conv.mutConversion()) +
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

std::optional<Conversion> sema::computeConversion(ConversionKind kind,
                                                  QualType from,
                                                  Value const* constantValue,
                                                  QualType to) {
    if (from == to) {
        return Conversion(from,
                          to,
                          RefConversion::None,
                          MutConversion::None,
                          ObjectTypeConversion::None);
    }
    auto refConv = determineRefConv(kind, from, to);
    if (!refConv) {
        return std::nullopt;
    }
    auto mutConv = determineMutConv(kind, from, to);
    if (!mutConv) {
        return std::nullopt;
    }
    auto objConv = determineObjConv(kind,
                                    stripReference(from).get(),
                                    stripReference(to).get());
    /// If we can't find an implicit conversion and we have a constant value,
    /// we try to find an extended constant implicit conversion
    if (kind == ConversionKind::Implicit && !objConv && constantValue) {
        objConv = tryImplicitConstConv(constantValue,
                                       stripReference(from).get(),
                                       stripReference(to).get());
    }
    if (!objConv) {
        return std::nullopt;
    }
    if (!isCompatible(*refConv, *objConv)) {
        return std::nullopt;
    }
    return Conversion(from, to, *refConv, *mutConv, *objConv);
}

std::optional<Conversion> sema::computeConversion(ConversionKind kind,
                                                  QualType from,
                                                  QualType to) {
    return computeConversion(kind, from, nullptr, to);
}

std::optional<Conversion> sema::computeConversion(ConversionKind kind,
                                                  ast::Expression* expr,
                                                  QualType to) {
    return computeConversion(kind, expr->type(), expr->constantValue(), to);
}

/// Implementation of the `convert*` functions
static ast::Expression* convertImpl(ConversionKind kind,
                                    ast::Expression* expr,
                                    QualType to,
                                    DTorStack* dtors,
                                    Context& ctx,
                                    bool invokeCopyCtor = true) {
    auto conversion =
        computeConversion(kind, expr->type(), expr->constantValue(), to);
    if (!conversion) {
        ctx.issueHandler().push<BadTypeConversion>(*expr, to);
        return nullptr;
    }
    auto* result = insertConversion(expr, *conversion, ctx.symbolTable());
    if (!invokeCopyCtor) {
        return result;
    }
    auto* structType = dyncast<StructType const*>(to.get());
    if (!structType) {
        return result;
    }
    bool needCtorCall =
        conversion->objectConversion() == ObjectTypeConversion::None &&
        result->isRValue() && expr->isLValue();
    if (needCtorCall) {
        return copyValue(result, *dtors, ctx);
    }
    return result;
}

ast::Expression* sema::convertImplicitly(ast::Expression* expr,
                                         QualType to,
                                         DTorStack& dtors,
                                         Context& ctx) {
    return convertImpl(ConversionKind::Implicit, expr, to, &dtors, ctx);
}

ast::Expression* sema::convertExplicitly(ast::Expression* expr,
                                         QualType to,
                                         DTorStack& dtors,
                                         Context& ctx) {
    return convertImpl(ConversionKind::Explicit, expr, to, &dtors, ctx);
}

ast::Expression* sema::convertReinterpret(ast::Expression* expr,
                                          QualType to,
                                          Context& ctx) {
    return convertImpl(ConversionKind::Reinterpret, expr, to, nullptr, ctx);
}

ast::Expression* sema::convertToMutRef(ast::Expression* expr, Context& ctx) {
    auto& sym = ctx.symbolTable();
    auto ref = sym.reference(stripReference(expr->type()).toMut());
    return convertImpl(ConversionKind::Implicit, expr, ref, nullptr, ctx);
}

ast::Expression* sema::dereference(ast::Expression* expr, Context& ctx) {
    return convertImpl(ConversionKind::Implicit,
                       expr,
                       stripReference(expr->type()),
                       nullptr,
                       ctx,
                       /* invokeCopyCtor = */ false);
}

static QualType commonRef(SymbolTable& sym,
                          QualType commonObjType,
                          QualType a,
                          QualType b) {
    /// If `a` is a reference and `b` is not or vice versa, we return a the
    /// common base type as mutable
    if (!isRef(a) != !isRef(b)) {
        return commonObjType.toMut();
    }
    /// If none of `a` and `b` is a reference, we return a the common base type
    /// as is
    if (!isRef(a) || !isRef(b)) {
        return commonObjType;
    }
    /// We only reference-qualify the result if no base type conversion occured
    if (stripReference(a).get() != stripReference(b).get()) {
        return commonObjType.toMut();
    }
    /// Both types are references
    return sym.reference(commonObjType);
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

static QualType commonBase(SymbolTable& sym, QualType a, QualType b) {
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
        [&](PointerType const& a, PointerType const& b) -> PointerType const* {
            auto commonPointeeType = commonType(sym, a.base(), b.base());
            if (!commonPointeeType) {
                return nullptr;
            }
            return sym.pointer(commonPointeeType);
        },
        [&](ObjectType const& a, ObjectType const& b) -> ObjectType const* {
            return nullptr;
        }
    }; // clang-format on
    return QualType(commonObjType, commonMut);
}

QualType sema::commonType(SymbolTable& sym, QualType a, QualType b) {
    QualType commonObjType =
        commonBase(sym, stripReference(a), stripReference(b));
    if (!commonObjType) {
        return nullptr;
    }
    return commonRef(sym, commonObjType, a, b);
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

static Entity* getConvertedEntity(Entity* original,
                                  Conversion const& conv,
                                  SymbolTable& sym) {
    if (conv.objectConversion() == ObjectTypeConversion::None) {
        return original;
    }
    return sym.temporary(conv.targetType());
}

ast::Expression* sema::insertConversion(ast::Expression* expr,
                                        Conversion const& conv,
                                        SymbolTable& sym) {
    SC_ASSERT(expr->parent(),
              "Can't insert a conversion if node has no parent");
    if (conv.isNoop()) {
        return expr;
    }
    size_t const indexInParent = expr->indexInParent();
    auto* parent = expr->parent();
    auto targetType = conv.targetType();
    auto owner = allocate<ast::Conversion>(expr->extractFromParent(),
                                           std::make_unique<Conversion>(conv));
    auto* result = owner.get();
    parent->setChild(indexInParent, std::move(owner));
    auto* entity = getConvertedEntity(expr->entity(), conv, sym);
    result->decorateExpr(entity, targetType);
    result->setConstantValue(
        evalConversion(result->conversion(),
                       result->expression()->constantValue()));
    return result;
}
