#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>

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
    auto* parent               = expr->parent();
    auto* targetType           = conv->targetType();
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
                return Signed_Widen;
            }
            return std::nullopt;
        }
        /// ** `Signed -> Unsigned` **
        return std::nullopt;
    }
    if (to.isSigned()) {
        /// ** `Unsigned -> Signed` **
        if (from.bitwidth() < to.bitwidth()) {
            return Unsigned_Widen;
        }
        return std::nullopt;
    }
    /// ** `Unsigned -> Unsigned` **
    if (from.bitwidth() <= to.bitwidth()) {
        return Unsigned_Widen;
    }
    return std::nullopt;
}

static std::optional<ObjectTypeConversion> explicitIntConversion(
    IntType const& from, IntType const& to) {
    using enum ObjectTypeConversion;
    if (from.bitwidth() == to.bitwidth()) {
        return None;
    }
    if (from.bitwidth() < to.bitwidth()) {
        if (from.isSigned()) {
            return Signed_Widen;
        }
        return Unsigned_Widen;
    }
    return Int_Trunc;
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
                return Int_Trunc;
            case ConvKind::Reinterpret:
                if (from.bitwidth() != 8) {
                    return std::nullopt;
                }
                return None;
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
                return None;
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
            /*  Explicit */ { std::nullopt, std::nullopt, None,         },
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(toSlim(from))]
                           [static_cast<size_t>(toSlim(to))];
    }
    case ConvKind::Explicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[5][5] = {
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
    auto toBaseMut   = baseMutability(to);
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
            auto* iTo   = dyncast<IntType const*>(to);
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
        case Int_Trunc:
            return fits(cast<IntValue const*>(value)->value(),
                        cast<ArithmeticType const*>(to)->bitwidth(),
                        cast<ArithmeticType const*>(to)->isSigned());

        case Signed_Widen: {
            auto intVal = cast<IntValue const*>(value)->value();
            return !intVal.negative();
        }

        case Unsigned_Widen:
            SC_DEBUGFAIL();

        case Float_Trunc:
            SC_DEBUGFAIL();

        case Float_Widen:
            SC_DEBUGFAIL();

        case SignedToFloat:
            [[fallthrough]];
        case UnsignedToFloat:
            [[fallthrough]];
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
                    ast::Expression const* expr,
                    QualType const* to) {
    QualType const* from = expr->type();
    Value const* value   = expr->constantValue();
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
    if (kind == ConvKind::Implicit && !objConv && value &&
        *refConv != RefConversion::TakeAddress)
    {
        objConv = tryImplicitConstConv(value, from->base(), to->base());
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
    auto checkResult = checkConversion(kind, expr, to);
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
                                             ast::Expression const* expr,
                                             QualType const* to) {
    auto conv = checkConversion(kind, expr, to);
    if (!conv) {
        return std::nullopt;
    }
    return getRank(std::get<0>(*conv), std::get<1>(*conv), std::get<2>(*conv));
}

std::optional<int> sema::implicitConversionRank(ast::Expression const* expr,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Implicit, expr, to);
}

std::optional<int> sema::explicitConversionRank(ast::Expression const* expr,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Explicit, expr, to);
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

static QualType const* commonTypeRefImpl(QualType const* a, QualType const* b) {
    SC_ASSERT(a->base() == b->base(),
              "Here we only deduce common reference qualification");
#warning Handle mutability here
    if (a->isExplicitRef()) {
        return b;
    }
    if (a->isImplicitRef()) {
        if (b->isReference()) {
            return a;
        }
        return b;
    }
    if (!b->isReference()) {
        return a;
    }
    return nullptr;
}

QualType const* sema::commonType(QualType const* a, QualType const* b) {
    if (a->base() != b->base()) {
        return nullptr;
    }
    if (auto* result = commonTypeRefImpl(a, b)) {
        return result;
    }
    return commonTypeRefImpl(b, a);
}
