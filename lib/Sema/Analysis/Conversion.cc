#include "Sema/Analysis/Conversion.h"

#include <optional>
#include <ostream>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(ObjectTypeConversion conv) {
    switch (conv) {
        using enum ObjectTypeConversion;
    case None:
        return "None";
    case Array_FixedToDynamic:
        return "Array_FixedToDynamic";
    case Int_Trunc:
        return "Int_Trunc";
    case Signed_Widen:
        return "Signed_Widen";
    case Unsigned_Widen:
        return "Unsigned_Widen";
    case Float_Trunc:
        return "Float_Trunc";
    case Float_Widen:
        return "Float_Widen";
    case SignedToFloat:
        return "SignedToFloat";
    case UnsignedToFloat:
        return "UnsignedToFloat";
    case FloatToSigned:
        return "FloatToSigned";
    case FloatToUnsigned:
        return "FloatToUnsigned";
    };
}

std::ostream& sema::operator<<(std::ostream& ostream,
                               ObjectTypeConversion conv) {
    return ostream << toString(conv);
}

std::string_view sema::toString(RefConversion conv) {
    switch (conv) {
        using enum RefConversion;
    case None:
        return "None";
    case Dereference:
        return "Dereference";
    case TakeAddress:
        return "TakeAddress";
    }
}

std::ostream& sema::operator<<(std::ostream& ostream, RefConversion conv) {
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
    return result;
}

namespace {

enum class ConvKind {
    Implicit,
    Explicit,
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
    if (from.bitwidth() <= to.bitwidth()) {
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
            return Int_Trunc;
        },
        [&](IntType const& from, IntType const& to) -> RetType {
            return kind == ConvKind::Implicit ?
                implicitIntConversion(from, to) :
                explicitIntConversion(from, to);
        },
        [&](FloatType const& from, FloatType const& to) -> RetType {
            if (from.bitwidth() <= to.bitwidth()) {
                return Float_Widen;
            }
            return kind == ConvKind::Implicit ?
                std::nullopt :
                std::optional(Float_Trunc);
        },
        [&](IntType const& from, FloatType const& to) -> RetType {
            if (kind == ConvKind::Implicit) {
                return std::nullopt;
            }
            if (from.isSigned()) {
                return SignedToFloat;
            }
            return UnsignedToFloat;
        },
        [&](FloatType const& from, IntType const& to) -> RetType {
            if (kind == ConvKind::Implicit) {
                return std::nullopt;
            }
            if (to.isSigned()) {
                return FloatToSigned;
            }
            return FloatToUnsigned;
        },
        [&](ArrayType const& from, ArrayType const& to) -> RetType {
            if (from.elementType() == to.elementType() &&
                !from.isDynamic() && to.isDynamic())
            {
                return Array_FixedToDynamic;
            }
            return std::nullopt;
        },
        [&](ObjectType const& from, ObjectType const& to) -> RetType {
            return std::nullopt;
        }
    }); // clang-format on
}

static std::optional<RefConversion> determineRefConv(ConvKind kind,
                                                     Reference from,
                                                     Reference to) {
    using enum RefConversion;
    switch (kind) {
    case ConvKind::Implicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[3][3] = {
            /* From  / To -> None           Implicit      Explicit */
            /*     None */ { None,          TakeAddress,  std::nullopt   },
            /* Implicit */ { Dereference,   None,         std::nullopt   },
            /* Explicit */ { std::nullopt,  std::nullopt, None           }
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(from)][static_cast<size_t>(to)];
    }
    case ConvKind::Explicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[3][3] = {
            /* From  / To -> None           Implicit      Explicit */
            /*     None */ { None,          TakeAddress,  TakeAddress    },
            /* Implicit */ { Dereference,   None,         TakeAddress    },
            /* Explicit */ { Dereference,   Dereference,  None           }
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(from)][static_cast<size_t>(to)];
    }
    }
}

bool isCompatible(ObjectTypeConversion objConv, RefConversion refConv) {
    using enum ObjectTypeConversion;
    switch (refConv) {
    case RefConversion::None:
        return true;
        break;

    case RefConversion::Dereference:
        return true;
        break;

    case RefConversion::TakeAddress:
        return objConv == None || objConv == Array_FixedToDynamic;
    }
}

static int getRank(RefConversion conv) {
    switch (conv) {
        using enum RefConversion;
    case None:
        return 0;
    case Dereference:
        return 0;
    case TakeAddress:
        return 2;
    }
}

static int getRank(ObjectTypeConversion conv) {
    switch (conv) {
        using enum ObjectTypeConversion;
    case None:
        return 0;
    case Array_FixedToDynamic:
        return 1;
    case Int_Trunc:
        return 2;
    case Signed_Widen:
        return 1;
    case Unsigned_Widen:
        return 1;
    case Float_Trunc:
        return 2;
    case Float_Widen:
        return 1;
    case SignedToFloat:
        return 2;
    case UnsignedToFloat:
        return 2;
    case FloatToSigned:
        return 2;
    case FloatToUnsigned:
        return 2;
    }
}

static int getRank(RefConversion refConv, ObjectTypeConversion objConv) {
    return getRank(refConv) + getRank(objConv);
}

static std::optional<std::pair<RefConversion, ObjectTypeConversion>>
    checkConversion(ConvKind kind, QualType const* from, QualType const* to) {
    if (from == to) {
        return std::pair{ RefConversion::None, ObjectTypeConversion::None };
    }
    auto objConv = determineObjConv(kind, from->base(), to->base());
    if (!objConv) {
        return std::nullopt;
    }
    auto refConv = determineRefConv(kind, from->reference(), to->reference());
    if (!refConv) {
        return std::nullopt;
    }
    if (!isCompatible(*objConv, *refConv)) {
        return std::nullopt;
    }
    return std::pair{ *refConv, *objConv };
}

static bool convertImpl(ConvKind kind,
                        ast::Expression* expr,
                        QualType const* to,
                        IssueHandler* iss) {
    if (expr->type() == to) {
        return true;
    }
    auto checkResult = checkConversion(kind, expr->type(), to);
    if (!checkResult) {
        if (iss) {
            iss->push<BadTypeConversion>(*expr, to);
        }
        return false;
    }
    auto [refConv, objConv] = *checkResult;
    auto conv =
        std::make_unique<sema::Conversion>(expr->type(), to, refConv, objConv);
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

static std::optional<int> conversionRankImpl(ConvKind kind,
                                             QualType const* from,
                                             QualType const* to) {
    auto conv = checkConversion(kind, from, to);
    if (!conv) {
        return std::nullopt;
    }
    return getRank(conv->first, conv->second);
}

std::optional<int> sema::implicitConversionRank(QualType const* from,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Implicit, from, to);
}

std::optional<int> sema::explicitConversionRank(QualType const* from,
                                                QualType const* to) {
    return conversionRankImpl(ConvKind::Explicit, from, to);
}

bool sema::convertToExplicitRef(ast::Expression* expr,
                                SymbolTable& sym,
                                IssueHandler& issueHandler) {
    return convertExplicitly(expr,
                             sym.setReference(expr->type(),
                                              Reference::Explicit),
                             issueHandler);
}

bool sema::convertToImplicitRef(ast::Expression* expr,
                                SymbolTable& sym,
                                IssueHandler& issueHandler) {
    return convertImplicitly(expr,
                             sym.setReference(expr->type(),
                                              Reference::Implicit),
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
    if (a->isExplicitReference()) {
        return b;
    }
    if (a->isImplicitReference()) {
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
