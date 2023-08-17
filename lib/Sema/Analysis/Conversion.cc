#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>

#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
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

/// Insert a `Conversion` node between \p expr and it's parent
static ast::Conversion* insertConversion(
    ast::Expression* expr, std::unique_ptr<sema::Conversion> conv) {
    SC_ASSERT(expr->parent(),
              "Can't insert a conversion if node has no parent");
    size_t const indexInParent = expr->indexInParent();
    auto* parent = expr->parent();
    auto* targetType = conv->targetType();
    auto owner =
        allocate<ast::Conversion>(expr->extractFromParent(), std::move(conv));
    auto* result = owner.get();
    parent->setChild(indexInParent, std::move(owner));
    auto* entity = targetType->isReference() ? expr->entity() : nullptr;
    result->decorate(entity, targetType);
    result->setConstantValue(
        evalConversion(result->conversion(),
                       result->expression()->constantValue()));
    return result;
}

namespace {

enum class ConvKind {
    Implicit,
    Explicit,
    Reinterpret,
};

} // namespace

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
    ConvKind kind, ObjectType const* from, ObjectType const* to) {
    using enum ObjectTypeConversion;
    if (from == to) {
        return None;
    }
    using RetType = std::optional<ObjectTypeConversion>;
    // clang-format off
    return visit(*from, *to, utl::overload{
        [&](IntType const& from, ByteType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                if (from.isSigned()) {
                    return from.size() == to.size() ? SU_Widen : SU_Trunc;
                }
                return from.size() == to.size() ? UU_Widen : UU_Trunc;
            case ConvKind::Reinterpret:
                if (from.size() != to.size()) {
                    return Reinterpret_Value;
                }
                return None;
            }
        },
        [&](ByteType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                if (to.isSigned()) {
                    return US_Widen;
                }
                return UU_Widen;
            case ConvKind::Reinterpret:
                if (from.size() != to.size()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](IntType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return implicitIntConversion(from, to);
            case ConvKind::Explicit:
                return explicitIntConversion(from, to);
            case ConvKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](FloatType const& from, FloatType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return std::nullopt;
            case ConvKind::Explicit:
                if (from.bitwidth() <= to.bitwidth()) {
                    return Float_Widen;
                }
                return Float_Trunc;
            case ConvKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return None;
            }
        },
        [&](IntType const& from, FloatType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                return from.isSigned() ? SignedToFloat : UnsignedToFloat;
            case ConvKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](FloatType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                return to.isSigned() ? FloatToSigned : FloatToUnsigned;
            case ConvKind::Reinterpret:
                if (from.bitwidth() != to.bitwidth()) {
                    return std::nullopt;
                }
                return Reinterpret_Value;
            }
        },
        [&](ArrayType const& from, ArrayType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                [[fallthrough]];
            case ConvKind::Explicit:
                if (from.elementType() == to.elementType() &&
                    !from.isDynamic() && to.isDynamic())
                {
                    return Array_FixedToDynamic;
                }
                return std::nullopt;
            case ConvKind::Reinterpret: {
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
                        return Reinterpret_ArrayRef_FromByte;
                    },
                    [](ObjectType const&, ByteType const&) -> RetType {
                        return Reinterpret_ArrayRef_ToByte;
                    },
                    [](ObjectType const&, ObjectType const&) -> RetType {
                        return std::nullopt;
                    }
                });
            }
            }
        },
        [&](ObjectType const& from, ObjectType const& to) -> RetType {
            return std::nullopt;
        }
    }); // clang-format on
}

enum class SlimRef { None, Implicit, Explicit };

static SlimRef toSlim(Reference ref) {
    switch (ref) {
        using enum SlimRef;
    case Reference::None:
        return None;
    case Reference::ConstImplicit:
    case Reference::MutImplicit:
        return Implicit;
    case Reference::ConstExplicit:
    case Reference::MutExplicit:
        return Explicit;
    }
}

static std::optional<RefConversion> determineRefConv(ConvKind kind,
                                                     Reference from,
                                                     Reference to) {
    using enum RefConversion;
    switch (kind) {
    case ConvKind::Implicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[3][3] = {
            /* From  / To     None          Implicit      Explicit      */
            /*      None */ { None,         TakeAddress,  std::nullopt, },
            /*  Implicit */ { Dereference,  None,         std::nullopt, },
            /*  Explicit */ { DerefExpl,    DerefExpl,    None,         },
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(toSlim(from))]
                           [static_cast<size_t>(toSlim(to))];
    }
    case ConvKind::Explicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[3][3] = {
            /* From  / To     None         Implicit    Explicit      */
            /*      None */ { None,        TakeAddress, TakeAddress, },
            /*  Implicit */ { Dereference, None,        TakeAddress, },
            /*  Explicit */ { Dereference, Dereference, None,        },
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(toSlim(from))]
                           [static_cast<size_t>(toSlim(to))];
    }
    case ConvKind::Reinterpret:
        if (from == to) {
            return None;
        }
        return std::nullopt;
    }
}

static std::optional<MutConversion> determineMutConv(ConvKind kind,
                                                     QualType const* from,
                                                     QualType const* to) {
    /// Conversions to values are not concerned with mutability restrictions
    if (!to->isReference()) {
        return MutConversion::None;
    }
    auto fromBaseMut = baseMutability(from);
    auto toBaseMut = baseMutability(to);
    /// No mutability conversion happens
    if (fromBaseMut == toBaseMut) {
        return MutConversion::None;
    }
    switch (fromBaseMut) {
    case Mutability::Mutable: // -> Const
        return MutConversion::MutToConst;
    case Mutability::Const:   // -> Mutable
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

    case RefConversion::DerefExpl:
        return true;

    case RefConversion::TakeAddress:
        return objConv == None || objConv == Array_FixedToDynamic;
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

static int getRank(RefConversion refConv,
                   MutConversion mutConv,
                   ObjectTypeConversion objConv) {
    return getRank(refConv) + getRank(mutConv) + getRank(objConv);
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
    auto result = determineObjConv(ConvKind::Explicit, from, to);
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

static std::optional<
    std::tuple<RefConversion, MutConversion, ObjectTypeConversion>>
    checkConversion(ConvKind kind,
                    sema::QualType const* from,
                    Value const* constantValue,
                    QualType const* to) {
    if (from == to) {
        return std::tuple{ RefConversion::None,
                           MutConversion::None,
                           ObjectTypeConversion::None };
    }
    auto refConv = determineRefConv(kind, from->reference(), to->reference());
    if (!refConv) {
        return std::nullopt;
    }
    auto mutConv = determineMutConv(kind, from, to);
    if (!mutConv) {
        return std::nullopt;
    }
    auto objConv = determineObjConv(kind, from->base(), to->base());
    /// If we can't find an implicit conversion and we have a constant value,
    /// we try to find an extended constant implicit conversion
    if (kind == ConvKind::Implicit && !objConv && constantValue &&
        *refConv != RefConversion::TakeAddress)
    {
        objConv = tryImplicitConstConv(constantValue, from->base(), to->base());
    }
    if (!objConv) {
        return std::nullopt;
    }
    if (!isCompatible(*refConv, *objConv)) {
        return std::nullopt;
    }
    return std::tuple{ *refConv, *mutConv, *objConv };
}

/// Implementation of the `convert*` functions
static bool convertImpl(ConvKind kind,
                        ast::Expression* expr,
                        QualType const* to,
                        IssueHandler* iss) {
    if (expr->type() == to) {
        return true;
    }
    auto checkResult =
        checkConversion(kind, expr->type(), expr->constantValue(), to);
    if (!checkResult) {
        if (iss) {
            iss->push<BadTypeConversion>(*expr, to);
        }
        return false;
    }
    auto [refConv, mutConv, objConv] = *checkResult;
    auto conv = std::make_unique<sema::Conversion>(expr->type(),
                                                   to,
                                                   refConv,
                                                   mutConv,
                                                   objConv);
    insertConversion(expr, std::move(conv));
    return true;
}

bool sema::convertImplicitly(ast::Expression* expr,
                             QualType const* to,
                             IssueHandler& issueHandler) {
    return convertImpl(ConvKind::Implicit, expr, to, &issueHandler);
}

bool sema::convertExplicitly(ast::Expression* expr,
                             QualType const* to,
                             IssueHandler& issueHandler) {
    return convertImpl(ConvKind::Explicit, expr, to, &issueHandler);
}

bool sema::convertReinterpret(ast::Expression* expr,
                              QualType const* to,
                              IssueHandler& issueHandler) {
    return convertImpl(ConvKind::Reinterpret, expr, to, &issueHandler);
}

/// Implementation of the `*ConversionRank()` functions
static std::optional<int> conversionRankImpl(ConvKind kind,
                                             QualType const* from,
                                             Value const* constantValue,
                                             QualType const* to) {
    auto conv = checkConversion(kind, from, constantValue, to);
    if (!conv) {
        return std::nullopt;
    }
    return getRank(std::get<0>(*conv), std::get<1>(*conv), std::get<2>(*conv));
}

std::optional<int> sema::implicitConversionRank(QualType const* from,
                                                QualType const* to) {
    return implicitConversionRank(from, nullptr, to);
}

std::optional<int> sema::implicitConversionRank(QualType const* from,
                                                Value const* constVal,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Implicit, from, constVal, to);
}

std::optional<int> sema::implicitConversionRank(ast::Expression const* expr,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Implicit,
                              expr->type(),
                              expr->constantValue(),
                              to);
}

std::optional<int> sema::explicitConversionRank(QualType const* from,
                                                QualType const* to) {
    return explicitConversionRank(from, nullptr, to);
}

std::optional<int> sema::explicitConversionRank(QualType const* from,
                                                Value const* constVal,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Explicit, from, constVal, to);
}

std::optional<int> sema::explicitConversionRank(ast::Expression const* expr,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Explicit,
                              expr->type(),
                              expr->constantValue(),
                              to);
}

bool sema::convertToExplicitRef(ast::Expression* expr,
                                SymbolTable& sym,
                                IssueHandler& issueHandler) {
    auto refQual = toExplicitRef(baseMutability(expr->type()));
    return convertExplicitly(expr,
                             sym.setReference(expr->type(), refQual),
                             issueHandler);
}

bool sema::convertToImplicitMutRef(ast::Expression* expr,
                                   SymbolTable& sym,
                                   IssueHandler& issueHandler) {
    return convertImplicitly(expr,
                             sym.setReference(expr->type(), RefMutImpl),
                             issueHandler);
}

void sema::dereference(ast::Expression* expr, SymbolTable& sym) {
    bool succ = convertImpl(ConvKind::Implicit,
                            expr,
                            sym.setReference(expr->type(), Reference::None),
                            nullptr);
    SC_ASSERT(succ, "Expression is not dereferentiable");
}

static std::optional<Reference> commonRef(Reference a, Reference b) {
    using enum Reference;
    // clang-format off
    static constexpr std::optional<Reference> resultMatrix[5][5] = {
/*                    None,         ConstImplicit  MutImplicit    ConstExplicit  MutExplicit   */
/*          None */ { None,         None,          None,          std::nullopt,  std::nullopt  },
/* ConstImplicit */ { None,         ConstImplicit, ConstImplicit, std::nullopt,  std::nullopt  },
/*   MutImplicit */ { None,         ConstImplicit, MutImplicit,   std::nullopt,  std::nullopt  },
/* ConstExplicit */ { std::nullopt, std::nullopt,  std::nullopt,  ConstExplicit, ConstExplicit },
/*   MutExplicit */ { std::nullopt, std::nullopt,  std::nullopt,  ConstExplicit, MutExplicit   },
    }; // clang-format on
    return resultMatrix[static_cast<size_t>(a)][static_cast<size_t>(b)];
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
        return sym.rawIntType(*width, Signedness::Signed);
    }
    return nullptr;
}

static ObjectType const* commonBase(SymbolTable& sym,
                                    ObjectType const* a,
                                    ObjectType const* b) {
    if (a == b) {
        return a;
    }
    // clang-format off
    return visit(*a, *b, utl::overload{
        [&](IntType const& a, IntType const& b) -> ObjectType const* {
            if (a.isSigned() && b.isUnsigned()) {
                return commonTypeSignedUnsigned(sym, &a, &b);
            }
            if (b.isSigned() && a.isUnsigned()) {
                return commonTypeSignedUnsigned(sym, &b, &a);
            }
            SC_ASSERT(a.signedness() == b.signedness(), "");
            return sym.rawIntType(std::max(a.bitwidth(), b.bitwidth()),
                               a.signedness());
        },
        [&](ObjectType const& a, ObjectType const& b) -> ObjectType const* {
            return nullptr;
        }
    }); // clang-format on
}

QualType const* sema::commonType(SymbolTable& sym,
                                 QualType const* a,
                                 QualType const* b) {
    auto* base = commonBase(sym, a->base(), b->base());
    if (!base) {
        return nullptr;
    }
    auto ref = commonRef(a->reference(), b->reference());
    if (!ref) {
        return nullptr;
    }
    /// We only reference-qualify the result if no base type conversion occurs
    if (a->base() == b->base()) {
        return sym.qualify(base, *ref);
    }
    return sym.qualify(base);
}

QualType const* sema::commonType(SymbolTable& sym,
                                 std::span<QualType const* const> types) {
    if (types.empty()) {
        return sym.Void();
    }
    auto* result = types[0];
    for (auto* type: types | ranges::views::drop(1)) {
        if (!result) {
            break;
        }
        result = commonType(sym, result, type);
    }
    return result;
}

QualType const* sema::commonType(
    SymbolTable& sym, std::span<ast::Expression const* const> exprs) {
    return commonType(sym, exprs | ranges::views::transform([](auto* expr) {
                               return expr->type();
                           }) | ranges::to<utl::small_vector<QualType const*>>);
}
