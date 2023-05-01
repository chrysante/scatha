#include "Sema/Conversion.h"

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

bool sema::isImplicitlyConvertible(QualType const* to, QualType const* from) {
    /// Can't convert non-arrays to arrays and vice versa
    if (from->base()->entityType() != to->base()->entityType()) {
        return false;
    }
    /// Can't implicitly make a reference from a non reference value
    if (to->isReference() && !from->isReference()) {
        return false;
    }
    if (to->isExplicitReference() && !from->isExplicitReference()) {
        return false;
    }
    return from->base() == to->base();
}

ast::ImplicitConversion* sema::insertImplicitConversion(
    ast::Expression* expr, sema::QualType const* toType) {
    SC_ASSERT(isImplicitlyConvertible(toType, expr->type()), "");
    size_t const indexInParent = expr->indexInParent();
    auto* parent               = expr->parent();
    auto owner =
        allocate<ast::ImplicitConversion>(expr->extractFromParent(), toType);
    auto* conv = owner.get();
    parent->setChild(indexInParent, std::move(owner));
    auto* entity = toType->isReference() ? expr->entity() : nullptr;
    conv->decorate(entity, toType);
    return conv;
}

static QualType const* commonTypeImpl(QualType const* a, QualType const* b) {
    SC_ASSERT(a->base() == b->base(), "");
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
    if (auto* result = commonTypeImpl(a, b)) {
        return result;
    }
    return commonTypeImpl(b, a);
}

bool sema::convertImplicitly(ast::Expression* expr,
                             QualType const* to,
                             IssueHandler& issueHandler) {
    if (expr->type() == to) {
        return true;
    }
    if (expr->type() && isImplicitlyConvertible(to, expr->type())) {
        insertImplicitConversion(expr, to);
        return true;
    }
    issueHandler.push<BadTypeConversion>(*expr, to);
    return false;
}
