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
concept DerivedFrom = std::derived_from<std::remove_cvref_t<Derived>, Base>;

template <typename T, typename U>
concept AlmostSameAs = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

} // namespace scatha::ast::internal

namespace scatha::ast {

decltype(auto) visit(internal::DerivedFrom<AbstractSyntaxTree> auto&& node, auto const& visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node)&&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), node.nodeType(), visitor);
}

decltype(auto) visit(internal::DerivedFrom<AbstractSyntaxTree> auto&& node, NodeType type, auto const& visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node)&&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), type, visitor);
}

namespace internal {
template <typename F>
struct DefaultCase {
    F callback;

    explicit DefaultCase(F callback): callback(callback) {}

    void operator()(AbstractSyntaxTree const&) const {}
    void operator()(AlmostSameAs<TranslationUnit> auto&& tu) const {
        for (auto&& decl : tu.declarations) {
            callback(*decl);
        }
    }
    void operator()(AlmostSameAs<Block> auto&& b) const {
        for (auto& s : b.statements) {
            callback(*s);
        }
    }
    void operator()(AlmostSameAs<FunctionDefinition> auto&& fn) const {
        for (auto& param : fn.parameters) {
            callback(*param);
        }
        callback(*fn.body);
    }
    void operator()(AlmostSameAs<StructDefinition> auto&& s) const { callback(*s.body); }
    void operator()(AlmostSameAs<VariableDeclaration> auto&& s) const {
        if (s.typeExpr) {
            callback(*s.typeExpr);
        }
        if (s.initExpression) {
            callback(*s.initExpression);
        }
    }
    void operator()(AlmostSameAs<ExpressionStatement> auto&& s) const {
        SC_ASSERT(s.expression != nullptr, "");
        callback(*s.expression);
    }
    void operator()(AlmostSameAs<ReturnStatement> auto&& s) const {
        if (s.expression) {
            callback(*s.expression);            
        }
    }
    void operator()(AlmostSameAs<IfStatement> auto&& s) const {
        callback(*s.condition);
        callback(*s.ifBlock);
        if (s.elseBlock != nullptr) {
            callback(*s.elseBlock);
        }
    }
    void operator()(AlmostSameAs<WhileStatement> auto&& s) const {
        callback(*s.condition);
        callback(*s.block);
    }
    void operator()(AlmostSameAs<IntegerLiteral> auto&&) const {}
    void operator()(AlmostSameAs<BooleanLiteral> auto&&) const {}
    void operator()(AlmostSameAs<FloatingPointLiteral> auto&&) const {}
    void operator()(AlmostSameAs<StringLiteral> auto&&) const {}
    void operator()(AlmostSameAs<UnaryPrefixExpression> auto&& e) const { callback(*e.operand); }
    void operator()(AlmostSameAs<BinaryExpression> auto&& e) const {
        callback(*e.lhs);
        callback(*e.rhs);
    }
    void operator()(AlmostSameAs<MemberAccess> auto&& m) const { callback(*m.object); }
    void operator()(AlmostSameAs<Conditional> auto&& c) const {
        callback(*c.condition);
        callback(*c.ifExpr);
        callback(*c.elseExpr);
    }
    void operator()(AlmostSameAs<FunctionCall> auto&& f) const {
        callback(*f.object);
        for (auto& arg : f.arguments) {
            callback(*arg);
        }
    }
    void operator()(AlmostSameAs<Subscript> auto&& s) const {
        callback(*s.object);
        for (auto& arg : s.arguments) {
            callback(*arg);
        }
    }
};
} // namespace internal

using internal::DefaultCase;

} // namespace scatha::ast

/// MARK: Implementation
namespace scatha::ast::internal {
decltype(auto) visitImpl(auto&& node, auto type, auto const& f) {
    static_assert(std::is_same_v<std::decay_t<decltype(node)>, AbstractSyntaxTree>);
    switch (type) {
    case NodeType::TranslationUnit: return f(static_cast<utl::copy_cvref_t<decltype(node), TranslationUnit>>(node));
    case NodeType::Block: return f(static_cast<utl::copy_cvref_t<decltype(node), Block>>(node));
    case NodeType::FunctionDefinition:
        return f(static_cast<utl::copy_cvref_t<decltype(node), FunctionDefinition>>(node));
    case NodeType::StructDefinition: return f(static_cast<utl::copy_cvref_t<decltype(node), StructDefinition>>(node));
    case NodeType::VariableDeclaration:
        return f(static_cast<utl::copy_cvref_t<decltype(node), VariableDeclaration>>(node));
    case NodeType::ExpressionStatement:
        return f(static_cast<utl::copy_cvref_t<decltype(node), ExpressionStatement>>(node));
    case NodeType::ReturnStatement: return f(static_cast<utl::copy_cvref_t<decltype(node), ReturnStatement>>(node));
    case NodeType::IfStatement: return f(static_cast<utl::copy_cvref_t<decltype(node), IfStatement>>(node));
    case NodeType::WhileStatement: return f(static_cast<utl::copy_cvref_t<decltype(node), WhileStatement>>(node));
    case NodeType::Identifier: return f(static_cast<utl::copy_cvref_t<decltype(node), Identifier>>(node));
    case NodeType::IntegerLiteral: return f(static_cast<utl::copy_cvref_t<decltype(node), IntegerLiteral>>(node));
    case NodeType::BooleanLiteral: return f(static_cast<utl::copy_cvref_t<decltype(node), BooleanLiteral>>(node));
    case NodeType::FloatingPointLiteral:
        return f(static_cast<utl::copy_cvref_t<decltype(node), FloatingPointLiteral>>(node));
    case NodeType::StringLiteral: return f(static_cast<utl::copy_cvref_t<decltype(node), StringLiteral>>(node));
    case NodeType::UnaryPrefixExpression:
        return f(static_cast<utl::copy_cvref_t<decltype(node), UnaryPrefixExpression>>(node));
    case NodeType::BinaryExpression: return f(static_cast<utl::copy_cvref_t<decltype(node), BinaryExpression>>(node));
    case NodeType::MemberAccess: return f(static_cast<utl::copy_cvref_t<decltype(node), MemberAccess>>(node));
    case NodeType::Conditional: return f(static_cast<utl::copy_cvref_t<decltype(node), Conditional>>(node));
    case NodeType::FunctionCall: return f(static_cast<utl::copy_cvref_t<decltype(node), FunctionCall>>(node));
    case NodeType::Subscript: return f(static_cast<utl::copy_cvref_t<decltype(node), Subscript>>(node));
    case NodeType::_count: SC_DEBUGFAIL();
    }
}
} // namespace scatha::ast::internal

#endif // SCATHA_AST_VISIT_H_
