#include "IC/Canonicalize.h"

#include <utl/utility.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Basic/Basic.h"

namespace scatha::ic {

namespace {

struct Context {
    void dispatch(ast::AbstractSyntaxTree&);

    void canonicalize(auto&);
    void canonicalize(ast::IfStatement&);
    void canonicalize(ast::BinaryExpression&);

    void doCompoundAssignment(ast::BinaryExpression& node, ast::BinaryOperator to) const;
};

} // namespace

void Context::dispatch(ast::AbstractSyntaxTree& node) {
    return ast::visit(node, [this](auto& node) { canonicalize(node); });
}

void Context::canonicalize(auto& node) {
    ast::DefaultCase([this](ast::AbstractSyntaxTree& node) { dispatch(node); })(node);
}

void Context::canonicalize(ast::IfStatement& statement) {
    dispatch(*statement.condition);
    dispatch(*statement.ifBlock);
    if (statement.elseBlock != nullptr) {
        dispatch(*statement.elseBlock);
    }
    /// If we have a statement of type \code if (!<boolean-expr>) {} else {} \endcode
    /// convert the expression to \p <boolean-expr> and swap the if
    /// and else block. 
    ///
    if (statement.condition->nodeType() != ast::NodeType::UnaryPrefixExpression) {
        return;
    }
    auto& un = *static_cast<ast::UnaryPrefixExpression*>(statement.condition.get());
    if (un.op != ast::UnaryPrefixOperator::LogicalNot) {
        return;
    }
    statement.condition = std::move(un.operand);
    std::swap(statement.ifBlock, statement.elseBlock);
}

void Context::canonicalize(ast::BinaryExpression& expr) {
    dispatch(*expr.lhs);
    dispatch(*expr.rhs);

    switch (expr.op) {
    case ast::BinaryOperator::Greater: {
        expr.op = ast::BinaryOperator::Less;
        std::swap(expr.lhs, expr.rhs);
        break;
    }
    case ast::BinaryOperator::GreaterEq: {
        expr.op = ast::BinaryOperator::LessEq;
        std::swap(expr.lhs, expr.rhs);
        break;
    }
    case ast::BinaryOperator::AddAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::Addition);
        break;
    }
    case ast::BinaryOperator::SubAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::Subtraction);
        break;
    }
    case ast::BinaryOperator::MulAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::Multiplication);
        break;
    }
    case ast::BinaryOperator::DivAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::Division);
        break;
    }
    case ast::BinaryOperator::RemAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::Remainder);
        break;
    }
    case ast::BinaryOperator::LSAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::LeftShift);
        break;
    }
    case ast::BinaryOperator::RSAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::RightShift);
        break;
    }
    case ast::BinaryOperator::AndAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::BitwiseAnd);
        break;
    }
    case ast::BinaryOperator::OrAssignment: {
        doCompoundAssignment(expr, ast::BinaryOperator::BitwiseOr);
        break;
    }

    default: break;
    }
}

void Context::doCompoundAssignment(ast::BinaryExpression& node, ast::BinaryOperator to) const {
    if (node.lhs->nodeType() != ast::NodeType::Identifier) {
        return;
    }
    auto& id = static_cast<ast::Identifier&>(*node.lhs);
    node.op  = ast::BinaryOperator::Assignment;
    node.rhs =
        ast::allocate<ast::BinaryExpression>(to, ast::allocate<ast::Identifier>(id), std::move(node.rhs), Token{});
}

void canonicalize(ast::AbstractSyntaxTree* node) {
    Context ctx;
    ctx.dispatch(*node);
}

} // namespace scatha::ic
