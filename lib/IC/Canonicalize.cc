#include "IC/Canonicalize.h"

#include "AST/Traversal.h"
#include "Basic/Basic.h"

namespace scatha::ic {
	
	namespace {
		struct Traverser {
			void post(ast::IfStatement* node) {
				/*
				 * If we have a statement of type if (!<boolean-expr>) {} else {}
				 * convert the expression to <boolean-expr> and swap the if and else block.
				 * These kind of if statements will appear quite often after canonicalizing the
				 * condition expressions.
				 */
				if (node->condition->nodeType() != ast::NodeType::UnaryPrefixExpression) {
					return;
				}
				auto* un = static_cast<ast::UnaryPrefixExpression*>(node->condition.get());
				if (un->op != ast::UnaryPrefixOperator::LogicalNot) {
					return;
				}
				node->condition = std::move(un->operand);
				std::swap(node->ifBlock, node->elseBlock);
			}
			void post(ast::BinaryExpression* node) {
				switch (node->op) {
					case ast::BinaryOperator::Greater: {
						node->op = ast::BinaryOperator::Less;
						std::swap(node->lhs, node->rhs);
						break;
					}
					case ast::BinaryOperator::GreaterEq: {
						node->op = ast::BinaryOperator::LessEq;
						std::swap(node->lhs, node->rhs);
						break;
					}
					case ast::BinaryOperator::AddAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::Addition);
						break;
					}
					case ast::BinaryOperator::SubAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::Subtraction);
						break;
					}
					case ast::BinaryOperator::MulAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::Multiplication);
						break;
					}
					case ast::BinaryOperator::DivAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::Division);
						break;
					}
					case ast::BinaryOperator::RemAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::Remainder);
						break;
					}
					case ast::BinaryOperator::LSAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::LeftShift);
						break;
					}
					case ast::BinaryOperator::RSAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::RightShift);
						break;
					}
					case ast::BinaryOperator::AndAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::BitwiseAnd);
						break;
					}
					case ast::BinaryOperator::OrAssignment: {
						doCompoundAssignment(node, ast::BinaryOperator::BitwiseOr);
						break;
					}
						
					default:
						break;
				}
			}
			void doCompoundAssignment(ast::BinaryExpression* node,
									  ast::BinaryOperator to) const
			{
				if (node->lhs->nodeType() != ast::NodeType::Identifier) {
					return;
				}
				auto* const id = static_cast<ast::Identifier*>(node->lhs.get());
				node->op = ast::BinaryOperator::Assignment;
				node->rhs = ast::allocate<ast::BinaryExpression>(to,
																 ast::allocate<ast::Identifier>(*id),
																 std::move(node->rhs),
																 Token{});
			}
		};
	}
	
	void canonicalize(ast::AbstractSyntaxTree* node) {
		ast::traverse(node, Traverser{});
	}
	
}
