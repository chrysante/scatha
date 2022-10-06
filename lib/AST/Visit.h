#ifndef SCATHA_AST_VISIT_H_
#define SCATHA_AST_VISIT_H_

#include <concepts>
#include <type_traits>

#include <utl/type_traits.hpp>
#include <utl/utility.hpp>

#include "AST/AST.h"
#include "AST/Base.h"
#include "AST/Common.h"
#include "AST/Fwd.h"
#include "Basic/Basic.h"

namespace scatha::ast::internal {
decltype(auto) visitImpl(auto &&, auto, auto const &);

template <typename Derived, typename Base>
concept DerivedFrom = std::derived_from<std::remove_cvref_t<Derived>, Base>;

template <typename T, typename U>
concept AlmostSameAs = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

} // namespace scatha::ast::internal

namespace scatha::ast {

decltype(auto) visit(internal::DerivedFrom<AbstractSyntaxTree> auto &&node, auto const &visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node) &&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), node.nodeType(), visitor);
}

decltype(auto) visit(internal::DerivedFrom<AbstractSyntaxTree> auto &&node, NodeType type, auto const &visitor) {
    using AstBaseType = utl::copy_cvref_t<decltype(node) &&, AbstractSyntaxTree>;
    return internal::visitImpl(static_cast<AstBaseType>(node), type, visitor);
}

auto constexpr DefaultCase(auto &&callback) {
    using namespace internal;
    return utl::visitor{[&](AlmostSameAs<TranslationUnit> auto &&tu) {
                            for (auto &&decl : tu.declarations) {
                                callback(*decl);
                            }
                        },
                        [&](AlmostSameAs<Block> auto &&b) {
                            for (auto &s : b.statements) {
                                callback(*s);
                            }
                        },
                        [&](AlmostSameAs<FunctionDefinition> auto &&fn) {
                            for (auto &param : fn.parameters) {
                                callback(*param);
                            }
                            callback(*fn.body);
                        },
                        [&](AlmostSameAs<StructDefinition> auto &&s) { callback(*s.body); },
                        [&](AlmostSameAs<VariableDeclaration> auto &&v) {
                            if (v.initExpression != nullptr) {
                                callback(*v.initExpression);
                            }
                        },
                        [&](AlmostSameAs<ExpressionStatement> auto &&s) {
                            SC_ASSERT(s.expression != nullptr, "");
                            callback(*s.expression);
                        },
                        [&](AlmostSameAs<ReturnStatement> auto &&s) { callback(*s.expression); },
                        [&](AlmostSameAs<IfStatement> auto &&s) {
                            callback(*s.condition);
                            callback(*s.ifBlock);
                            if (s.elseBlock != nullptr) {
                                callback(*s.elseBlock);
                            }
                        },
                        [&](AlmostSameAs<WhileStatement> auto &&s) {
                            callback(*s.condition);
                            callback(*s.block);
                        },
                        [&](AlmostSameAs<Identifier> auto &&) {

                        },
                        [&](AlmostSameAs<IntegerLiteral> auto &&) {

                        },
                        [&](AlmostSameAs<BooleanLiteral> auto &&) {

                        },
                        [&](AlmostSameAs<FloatingPointLiteral> auto &&) {

                        },
                        [&](AlmostSameAs<StringLiteral> auto &&) {

                        },
                        [&](AlmostSameAs<UnaryPrefixExpression> auto &&e) { callback(*e.operand); },
                        [&](AlmostSameAs<BinaryExpression> auto &&e) {
                            callback(*e.lhs);
                            callback(*e.rhs);
                        },
                        [&](AlmostSameAs<MemberAccess> auto &&m) { callback(*m.object); },
                        [&](AlmostSameAs<Conditional> auto &&c) {
                            callback(*c.condition);
                            callback(*c.ifExpr);
                            callback(*c.elseExpr);
                        },
                        [&](AlmostSameAs<FunctionCall> auto &&f) {
                            callback(*f.object);
                            for (auto &arg : f.arguments) {
                                callback(*arg);
                            }
                        },
                        [&](AlmostSameAs<Subscript> auto &&s) {
                            callback(*s.object);
                            for (auto &arg : s.arguments) {
                                callback(*arg);
                            }
                        }};
}

} // namespace scatha::ast

/// MARK: Implementation

namespace scatha::ast::internal {
decltype(auto) visitImpl(auto &&node, auto type, auto const &f) {
    /// ---- NOTE: Confusing errors of references not being able to bind to
    /// unrelated types are usually caused by missing include of either
    /// "AST/AST.h" or "AST/Expression.h". Neither are included by this file but
    /// required to be used with this file.
    static_assert(std::is_same_v<std::decay_t<decltype(node)>, AbstractSyntaxTree>);
    switch (type) {
    case NodeType::TranslationUnit:
        return f(static_cast<utl::copy_cvref_t<decltype(node), TranslationUnit>>(node));
    case NodeType::Block:
        return f(static_cast<utl::copy_cvref_t<decltype(node), Block>>(node));
    case NodeType::FunctionDefinition:
        return f(static_cast<utl::copy_cvref_t<decltype(node), FunctionDefinition>>(node));
    case NodeType::StructDefinition:
        return f(static_cast<utl::copy_cvref_t<decltype(node), StructDefinition>>(node));
    case NodeType::VariableDeclaration:
        return f(static_cast<utl::copy_cvref_t<decltype(node), VariableDeclaration>>(node));
    case NodeType::ExpressionStatement:
        return f(static_cast<utl::copy_cvref_t<decltype(node), ExpressionStatement>>(node));
    case NodeType::ReturnStatement:
        return f(static_cast<utl::copy_cvref_t<decltype(node), ReturnStatement>>(node));
    case NodeType::IfStatement:
        return f(static_cast<utl::copy_cvref_t<decltype(node), IfStatement>>(node));
    case NodeType::WhileStatement:
        return f(static_cast<utl::copy_cvref_t<decltype(node), WhileStatement>>(node));
    case NodeType::Identifier:
        return f(static_cast<utl::copy_cvref_t<decltype(node), Identifier>>(node));
    case NodeType::IntegerLiteral:
        return f(static_cast<utl::copy_cvref_t<decltype(node), IntegerLiteral>>(node));
    case NodeType::BooleanLiteral:
        return f(static_cast<utl::copy_cvref_t<decltype(node), BooleanLiteral>>(node));
    case NodeType::FloatingPointLiteral:
        return f(static_cast<utl::copy_cvref_t<decltype(node), FloatingPointLiteral>>(node));
    case NodeType::StringLiteral:
        return f(static_cast<utl::copy_cvref_t<decltype(node), StringLiteral>>(node));
    case NodeType::UnaryPrefixExpression:
        return f(static_cast<utl::copy_cvref_t<decltype(node), UnaryPrefixExpression>>(node));
    case NodeType::BinaryExpression:
        return f(static_cast<utl::copy_cvref_t<decltype(node), BinaryExpression>>(node));
    case NodeType::MemberAccess:
        return f(static_cast<utl::copy_cvref_t<decltype(node), MemberAccess>>(node));
    case NodeType::Conditional:
        return f(static_cast<utl::copy_cvref_t<decltype(node), Conditional>>(node));
    case NodeType::FunctionCall:
        return f(static_cast<utl::copy_cvref_t<decltype(node), FunctionCall>>(node));
    case NodeType::Subscript:
        return f(static_cast<utl::copy_cvref_t<decltype(node), Subscript>>(node));
    case NodeType::_count:
        SC_DEBUGFAIL();
    }
}
} // namespace scatha::ast::internal

#endif // SCATHA_AST_VISIT_H_
