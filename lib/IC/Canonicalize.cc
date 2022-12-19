#include "IC/Canonicalize.h"

#include <utl/utility.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"

namespace scatha::ic {

struct Canonicalizer {
    void dispatch(ast::AbstractSyntaxTree&);

    void canonicalize(auto&);
    void canonicalize(ast::IfStatement&);
    void canonicalize(ast::BinaryExpression&);

    void doCompoundAssignment(ast::BinaryExpression& node, ast::BinaryOperator to) const;
};

} // namespace scatha::ic

using namespace scatha;
using namespace ic;

void Canonicalizer::dispatch(ast::AbstractSyntaxTree& node) {
    return visit(node, [this](auto& node) { canonicalize(node); });
}

void Canonicalizer::canonicalize(auto& node) {
    ast::DefaultVisitor([this](ast::AbstractSyntaxTree& node) { dispatch(node); })(node);
}

void Canonicalizer::canonicalize(ast::IfStatement& statement) {
    dispatch(*statement.condition);
    dispatch(*statement.ifBlock);
    if (statement.elseBlock != nullptr) {
        dispatch(*statement.elseBlock);
    }
    /// If we have a statement of type \code if (!<boolean-expr>) {} else {} \endcode
    /// convert the expression to \p <boolean-expr> and swap the if and else block.
    if (statement.condition->nodeType() != ast::NodeType::UnaryPrefixExpression) {
        return;
    }
    auto& un = *static_cast<ast::UnaryPrefixExpression*>(statement.condition.get());
    if (un.operation() != ast::UnaryPrefixOperator::LogicalNot) {
        return;
    }
    statement.condition = std::move(un.operand);
    std::swap(statement.ifBlock, statement.elseBlock);
}

void Canonicalizer::canonicalize(ast::BinaryExpression& expr) {
    dispatch(*expr.lhs);
    dispatch(*expr.rhs);
    switch (expr.operation()) {
    case ast::BinaryOperator::Greater: {
        expr.setOperation(ast::BinaryOperator::Less);
        std::swap(expr.lhs, expr.rhs);
        break;
    }
    case ast::BinaryOperator::GreaterEq: {
        expr.setOperation(ast::BinaryOperator::LessEq);
        std::swap(expr.lhs, expr.rhs);
        break;
    }
    case ast::BinaryOperator::AddAssignment: [[fallthrough]];
    case ast::BinaryOperator::SubAssignment: [[fallthrough]];
    case ast::BinaryOperator::MulAssignment: [[fallthrough]];
    case ast::BinaryOperator::DivAssignment: [[fallthrough]];
    case ast::BinaryOperator::RemAssignment: [[fallthrough]];
    case ast::BinaryOperator::LSAssignment: [[fallthrough]];
    case ast::BinaryOperator::RSAssignment: [[fallthrough]];
    case ast::BinaryOperator::AndAssignment: [[fallthrough]];
    case ast::BinaryOperator::OrAssignment:
        doCompoundAssignment(expr, toNonAssignment(expr.operation()));
        break;
    default: break;
    }
}

void Canonicalizer::doCompoundAssignment(ast::BinaryExpression& node, ast::BinaryOperator to) const {
    if (node.lhs->nodeType() != ast::NodeType::Identifier) {
        return;
    }
    auto& id = static_cast<ast::Identifier&>(*node.lhs);
    node.setOperation(ast::BinaryOperator::Assignment);
    node.rhs =
        ast::allocate<ast::BinaryExpression>(to, ast::allocate<ast::Identifier>(id), std::move(node.rhs), Token{});
}

void ic::canonicalize(ast::AbstractSyntaxTree* node) {
    Canonicalizer ctx;
    ctx.dispatch(*node);
}

