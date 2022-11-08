#ifndef SCATHA_AST_VISIT_H_
#define SCATHA_AST_VISIT_H_

#include <concepts>
#include <type_traits>

#include <utl/type_traits.hpp>
#include <utl/utility.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"

namespace scatha::ast::internal {
decltype(auto) visitImpl(auto&&, auto, auto const&);

template <typename Derived, typename Base>
concept WeakDerivedFrom = std::derived_from<std::remove_cvref_t<Derived>, Base>;

template <typename T, typename U>
concept WeakSameAs = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

} // namespace scatha::ast::internal

namespace scatha::ast {

decltype(auto) visit(internal::WeakDerivedFrom<AbstractSyntaxTree> auto&& node, auto const& visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node)&&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), node.nodeType(), visitor);
}

decltype(auto) visit(internal::WeakDerivedFrom<AbstractSyntaxTree> auto&& node, NodeType type, auto const& visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node)&&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), type, visitor);
}

namespace internal {
template <typename F>
struct DefaultCase {
    F callback;

    explicit DefaultCase(F callback): callback(callback) {}

    void operator()(AbstractSyntaxTree const&) const {}
    void operator()(WeakSameAs<TranslationUnit> auto&& tu) const {
        for (auto&& decl: tu.declarations) {
            std::invoke(callback, *decl);
        }
    }
    void operator()(WeakSameAs<CompoundStatement> auto&& b) const {
        for (auto& s: b.statements) {
            std::invoke(callback, *s);
        }
    }
    void operator()(WeakSameAs<FunctionDefinition> auto&& fn) const {
        for (auto& param: fn.parameters) {
            std::invoke(callback, *param);
        }
        std::invoke(callback, *fn.body);
    }
    void operator()(WeakSameAs<StructDefinition> auto&& s) const { std::invoke(callback, *s.body); }
    void operator()(WeakSameAs<VariableDeclaration> auto&& s) const {
        if (s.typeExpr) {
            std::invoke(callback, *s.typeExpr);
        }
        if (s.initExpression) {
            std::invoke(callback, *s.initExpression);
        }
    }
    void operator()(WeakSameAs<ExpressionStatement> auto&& s) const {
        SC_ASSERT(s.expression != nullptr, "");
        std::invoke(callback, *s.expression);
    }
    void operator()(WeakSameAs<ReturnStatement> auto&& s) const {
        if (s.expression) {
            std::invoke(callback, *s.expression);
        }
    }
    void operator()(WeakSameAs<IfStatement> auto&& s) const {
        std::invoke(callback, *s.condition);
        std::invoke(callback, *s.ifBlock);
        if (s.elseBlock != nullptr) {
            std::invoke(callback, *s.elseBlock);
        }
    }
    void operator()(WeakSameAs<WhileStatement> auto&& s) const {
        std::invoke(callback, *s.condition);
        std::invoke(callback, *s.block);
    }
    void operator()(WeakSameAs<IntegerLiteral> auto&&) const {}
    void operator()(WeakSameAs<BooleanLiteral> auto&&) const {}
    void operator()(WeakSameAs<FloatingPointLiteral> auto&&) const {}
    void operator()(WeakSameAs<StringLiteral> auto&&) const {}
    void operator()(WeakSameAs<UnaryPrefixExpression> auto&& e) const { std::invoke(callback, *e.operand); }
    void operator()(WeakSameAs<BinaryExpression> auto&& e) const {
        std::invoke(callback, *e.lhs);
        std::invoke(callback, *e.rhs);
    }
    void operator()(WeakSameAs<MemberAccess> auto&& m) const { callback(*m.object); }
    void operator()(WeakSameAs<Conditional> auto&& c) const {
        std::invoke(callback, *c.condition);
        std::invoke(callback, *c.ifExpr);
        std::invoke(callback, *c.elseExpr);
    }
    void operator()(WeakSameAs<FunctionCall> auto&& f) const {
        std::invoke(callback, *f.object);
        for (auto& arg: f.arguments) {
            std::invoke(callback, *arg);
        }
    }
    void operator()(WeakSameAs<Subscript> auto&& s) const {
        std::invoke(callback, *s.object);
        for (auto& arg: s.arguments) {
            std::invoke(callback, *arg);
        }
    }
};
} // namespace internal

using internal::DefaultCase;

void visitDefault(auto&& node, auto&& callback) {
    DefaultCase dc(callback);
    dc(node);
}

} // namespace scatha::ast

/// MARK: Implementation
namespace scatha::ast::internal {
decltype(auto) visitImpl(auto&& node, auto type, auto const& f) {
    static_assert(std::is_same_v<std::decay_t<decltype(node)>, AbstractSyntaxTree>);
    switch (type) {
    case NodeType::TranslationUnit: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), TranslationUnit>>(node));
    case NodeType::CompoundStatement:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), CompoundStatement>>(node));
    case NodeType::FunctionDefinition:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), FunctionDefinition>>(node));
    case NodeType::StructDefinition:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), StructDefinition>>(node));
    case NodeType::VariableDeclaration:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), VariableDeclaration>>(node));
    case NodeType::ParameterDeclaration:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), ParameterDeclaration>>(node));
    case NodeType::ExpressionStatement:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), ExpressionStatement>>(node));
    case NodeType::EmptyStatement: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), EmptyStatement>>(node));
    case NodeType::ReturnStatement: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), ReturnStatement>>(node));
    case NodeType::IfStatement: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), IfStatement>>(node));
    case NodeType::WhileStatement: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), WhileStatement>>(node));
    case NodeType::Identifier: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), Identifier>>(node));
    case NodeType::IntegerLiteral: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), IntegerLiteral>>(node));
    case NodeType::BooleanLiteral: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), BooleanLiteral>>(node));
    case NodeType::FloatingPointLiteral:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), FloatingPointLiteral>>(node));
    case NodeType::StringLiteral: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), StringLiteral>>(node));
    case NodeType::UnaryPrefixExpression:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), UnaryPrefixExpression>>(node));
    case NodeType::BinaryExpression:
        return f(utl::down_cast<utl::copy_cvref_t<decltype(node), BinaryExpression>>(node));
    case NodeType::MemberAccess: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), MemberAccess>>(node));
    case NodeType::Conditional: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), Conditional>>(node));
    case NodeType::FunctionCall: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), FunctionCall>>(node));
    case NodeType::Subscript: return f(utl::down_cast<utl::copy_cvref_t<decltype(node), Subscript>>(node));
    case NodeType::_count: SC_DEBUGFAIL();
    }
}
} // namespace scatha::ast::internal

#endif // SCATHA_AST_VISIT_H_
