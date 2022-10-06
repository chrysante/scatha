#include "IC/Canonicalize.h"

#include <utl/utility.hpp>

#include "AST/Expression.h"
#include "AST/Visit.h"
#include "Basic/Basic.h"

namespace scatha::ic {

namespace {
struct Context {
    void run(ast::AbstractSyntaxTree &, ast::NodeType);
    void run(ast::AbstractSyntaxTree &node) { run(node, node.nodeType()); }
    void doCompoundAssignment(ast::BinaryExpression &node, ast::BinaryOperator to) const;
};
} // namespace

void Context::run(ast::AbstractSyntaxTree &node, ast::NodeType type) {
    ast::visit(node, type,
               utl::visitor{[&](ast::IfStatement &is) {
                                run(*is.condition);
                                run(*is.ifBlock);
                                if (is.elseBlock != nullptr) {
                                    run(*is.elseBlock);
                                }

                                /*
                                 * If we have a statement of type if (!<boolean-expr>) {} else
                                 * {} convert the expression to <boolean-expr> and swap the if
                                 * and else block. These kind of if statements will appear quite
                                 * often after canonicalizing the condition expressions.
                                 */
                                if (is.condition->nodeType() != ast::NodeType::UnaryPrefixExpression) {
                                    return;
                                }
                                auto &un = *static_cast<ast::UnaryPrefixExpression *>(is.condition.get());
                                if (un.op != ast::UnaryPrefixOperator::LogicalNot) {
                                    return;
                                }
                                is.condition = std::move(un.operand);
                                std::swap(is.ifBlock, is.elseBlock);
                            },
                            [&](ast::BinaryExpression &expr) {
                                run(*expr.lhs);
                                run(*expr.rhs);

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

                                default:
                                    break;
                                }
                            },
                            ast::DefaultCase(utl::visitor{
                                [&](ast::AbstractSyntaxTree &node) { run(node); },
                                [&](ast::AbstractSyntaxTree &node, ast::NodeType type) { run(node, type); },
                            })});
}

void Context::doCompoundAssignment(ast::BinaryExpression &node, ast::BinaryOperator to) const {
    if (node.lhs->nodeType() != ast::NodeType::Identifier) {
        return;
    }
    auto &id = static_cast<ast::Identifier &>(*node.lhs);
    node.op  = ast::BinaryOperator::Assignment;
    node.rhs =
        ast::allocate<ast::BinaryExpression>(to, ast::allocate<ast::Identifier>(id), std::move(node.rhs), Token{});
}

void canonicalize(ast::AbstractSyntaxTree *node) {
    Context ctx;
    ctx.run(*node);
}

} // namespace scatha::ic
