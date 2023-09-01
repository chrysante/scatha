#include "AST/AST.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace ast;

void ast::privateDelete(AbstractSyntaxTree* node) {
    visit(*node, [](auto& derived) { delete &derived; });
}

static void extSourceRangeImpl(SourceRange& result,
                               AbstractSyntaxTree const* node) {
    result = merge(result, node->sourceRange());
    for (auto* child: node->children()) {
        if (child) {
            extSourceRangeImpl(result, child);
        }
    }
}

SourceRange AbstractSyntaxTree::extSourceRange() const {
    SourceRange result;
    extSourceRangeImpl(result, this);
    return result;
}

UniquePtr<AbstractSyntaxTree> AbstractSyntaxTree::extractFromParent() {
    return parent()->extractChild(indexInParent());
}

void AbstractSyntaxTree::replaceChild(AbstractSyntaxTree const* old,
                                      UniquePtr<AbstractSyntaxTree> repl) {
    auto itr = ranges::find_if(_children,
                               [&](auto& child) { return child.get() == old; });
    SC_ASSERT(itr != ranges::end(_children), "`old` is not a child of this");
    repl->_parent = this;
    *itr = std::move(repl);
}

size_t AbstractSyntaxTree::indexOf(AbstractSyntaxTree const* child) const {
    auto itr = ranges::find_if(_children, [&](auto& otherChild) {
        return otherChild.get() == child;
    });
    return utl::narrow_cast<size_t>(itr - _children.begin());
}

sema::QualType Expression::typeOrTypeEntity() const {
    return isValue() ? type() : cast<sema::ObjectType const*>(entity());
}

void Expression::decorate(sema::Entity* entity,
                          sema::QualType type,
                          std::optional<sema::ValueCategory> valueCat,
                          std::optional<sema::EntityCategory> entityCat) {
    _entity = entity;
    _type = type;
    /// Derive defaults
    if (entity) {
        // clang-format off
        _valueCat = SC_MATCH (*entity) {
            [](sema::Variable&) { return sema::ValueCategory::LValue; },
            [](sema::Temporary&) { return sema::ValueCategory::RValue; },
            [](sema::Entity&) { return sema::ValueCategory::None; },
        }; // clang-format on
        _entityCat = entity->category();
    }
    else {
        _valueCat = sema::ValueCategory::RValue;
        _entityCat = sema::EntityCategory::Value;
    }
    /// Override if user specified
    if (valueCat) {
        _valueCat = *valueCat;
    }
    if (entityCat) {
        _entityCat = *entityCat;
    }
    markDecorated();
}

Conversion::Conversion(UniquePtr<Expression> expr,
                       std::unique_ptr<sema::Conversion> conv):
    Expression(NodeType::Conversion, expr->sourceRange(), std::move(expr)),
    _conv(std::move(conv)) {}

Conversion::~Conversion() = default;

sema::QualType Conversion::targetType() const {
    return conversion()->targetType();
}

sema::ObjectType const* ConstructorCall::constructedType() const {
    return cast<sema::ObjectType const*>(function()->parent());
}
