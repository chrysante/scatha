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
    case Reinterpret_ArrayRef_ToByte:
        return "Reinterpret_ArrayRef_ToByte";
    case Reinterpret_ArrayRef_FromByte:
        return "Reinterpret_ArrayRef_FromByte";
    case Reinterpret_Value:
        return "Reinterpret_Value";
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
    case MutToConst:
        return "MutToConst";
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
                SC_DEBUGFAIL();
            }
        },
        [&](IntType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return implicitIntConversion(from, to);
            case ConvKind::Explicit:
                return explicitIntConversion(from, to);
            case ConvKind::Reinterpret:
                SC_DEBUGFAIL();
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
                SC_DEBUGFAIL();
            }
        },
        [&](IntType const& from, FloatType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                return from.isSigned() ? SignedToFloat : UnsignedToFloat;
            case ConvKind::Reinterpret:
                SC_DEBUGFAIL();
            }
        },
        [&](FloatType const& from, IntType const& to) -> RetType {
            switch (kind) {
            case ConvKind::Implicit:
                return std::nullopt;
            case ConvKind::Explicit:
                return to.isSigned() ? FloatToSigned : FloatToUnsigned;
            case ConvKind::Reinterpret:
                SC_DEBUGFAIL();
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

static std::optional<RefConversion> determineRefConv(ConvKind kind,
                                                     Reference from,
                                                     Reference to) {
    using enum RefConversion;
    switch (kind) {
    case ConvKind::Implicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[5][5] = {
            /* From  / To     None          ConstImpl     MutImpl       ConstExpl     MutExpl      */
            /*      None */ { None,         TakeAddress,  TakeAddress,  std::nullopt, std::nullopt },
            /* ConstImpl */ { Dereference,  None,         std::nullopt, std::nullopt, std::nullopt },
            /*   MutImpl */ { Dereference,  MutToConst,   None,         std::nullopt, std::nullopt },
            /* ConstExpl */ { std::nullopt, std::nullopt, std::nullopt, None,         std::nullopt },
            /*   MutExpl */ { std::nullopt, std::nullopt, std::nullopt, MutToConst,   None         },
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(from)][static_cast<size_t>(to)];
    }
    case ConvKind::Explicit: {
        // clang-format off
        static constexpr std::optional<RefConversion> resultMatrix[5][5] = {
            /* From  / To     None         ConstImpl    MutImpl       ConstExpl    MutExpl      */
            /*      None */ { None,        TakeAddress, TakeAddress,  TakeAddress, TakeAddress  },
            /* ConstImpl */ { Dereference, None,        std::nullopt, TakeAddress, std::nullopt },
            /*   MutImpl */ { Dereference, MutToConst,  None,         TakeAddress, TakeAddress  },
            /* ConstExpl */ { Dereference, Dereference, std::nullopt, None,        std::nullopt },
            /*   MutExpl */ { Dereference, Dereference, Dereference,  MutToConst,  None         },
        }; // clang-format on
        return resultMatrix[static_cast<size_t>(from)][static_cast<size_t>(to)];
    }
    case ConvKind::Reinterpret:
        if (from == to) {
            return None;
        }
        return std::nullopt;
    }
}

bool isCompatible(ObjectTypeConversion objConv, RefConversion refConv) {
    using enum ObjectTypeConversion;
    switch (refConv) {
    case RefConversion::None:
        return true;

    case RefConversion::MutToConst:
        return true;

    case RefConversion::Dereference:
        return true;

    case RefConversion::TakeAddress:
        return objConv == None || objConv == Array_FixedToDynamic;
    }
}

static int getRank(RefConversion conv) {
    switch (conv) {
        using enum RefConversion;
    case None:
        return 0;
    case MutToConst:
        return 1;
    case Dereference:
        return 1;
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
    case Reinterpret_ArrayRef_ToByte:
        return 2;
    case Reinterpret_ArrayRef_FromByte:
        return 2;
    case Reinterpret_Value:
        return 2;
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
    using enum Mutability;
    if (baseMutability(from) == Const && to->isMutRef()) {
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

bool sema::convertReinterpret(ast::Expression* expr,
                              QualType const* to,
                              IssueHandler& issueHandler) {
    return convertImpl(ConvKind::Reinterpret, expr, to, &issueHandler);
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
    auto refQual = toExplicitRef(baseMutability(expr->type()));
    return convertExplicitly(expr,
                             sym.setReference(expr->type(), refQual),
                             issueHandler);
}

bool sema::convertToImplicitRef(ast::Expression* expr,
                                SymbolTable& sym,
                                IssueHandler& issueHandler) {
    auto refQual = toImplicitRef(baseMutability(expr->type()));
    return convertImplicitly(expr,
                             sym.setReference(expr->type(), refQual),
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
