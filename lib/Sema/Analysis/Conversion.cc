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
    case Int_Widen:
        return "Int_Widen";
    case Int_WidenSigned:
        return "Int_WidenSigned";
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
static ast::Conversion* insertConversion(ast::Expression* expr,
                                         std::unique_ptr<Conversion> conv) {
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

static std::optional<ObjectTypeConversion> determineObjConv(
    ConvKind kind, ObjectType const* from, ObjectType const* to) {
    if (to == from) {
        return ObjectTypeConversion::None;
    }

    /// ## Integral conversions
    /// Coming later...

    /// ## Array conversions
    if (auto* toArray = dyncast<ArrayType const*>(to)) {
        auto* fromArray = dyncast<ArrayType const*>(from);
        if (!fromArray) {
            return std::nullopt;
        }
        if (fromArray->elementType() != toArray->elementType()) {
            return std::nullopt;
        }
        if (!fromArray->isDynamic() && toArray->isDynamic()) {
            return ObjectTypeConversion::Array_FixedToDynamic;
        }
        return std::nullopt;
    }

    return std::nullopt;
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

static bool convertImpl(ConvKind kind,
                        ast::Expression* expr,
                        QualType const* to,
                        IssueHandler* iss) {
    if (expr->type() == to) {
        return true;
    }
    auto pushIssue = [&] {
        if (!iss) {
            return;
        }
        iss->push<BadTypeConversion>(*expr, to);
    };

    auto objConv = determineObjConv(kind, expr->type()->base(), to->base());
    if (!objConv) {
        pushIssue();
        return false;
    }
    auto refConv =
        determineRefConv(kind, expr->type()->reference(), to->reference());
    if (!refConv) {
        pushIssue();
        return false;
    }
    if (!isCompatible(*objConv, *refConv)) {
        pushIssue();
        return false;
    }
    auto conv =
        std::make_unique<Conversion>(expr->type(), to, *refConv, *objConv);
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
