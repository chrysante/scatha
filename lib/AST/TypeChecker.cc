#include "TypeChecker.h"

#include <sstream>
#include <utl/strcat.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Common/Type.h"
#include "Common/TypeTable.h"

namespace scatha::ast {

	std::string TypeError::makeString(std::string_view brief, Token const& token, std::string_view message) {
		std::stringstream sstr;
		sstr << brief << " at Line: " << token.sourceLocation.line << " Col: " << token.sourceLocation.column;
		if (!message.empty()) {
			sstr << ": \n" << message;
		}
		return sstr.str();
	}
	
	ImplicitConversionError::ImplicitConversionError(TypeTable const& tbl, TypeID from, TypeID to, Token const& token):
		TypeError(utl::strcat("Cannot convert from ",
							  tbl.findByID(from).name(),
							  " to ",
							  tbl.findByID(to).name()),
				  token)
	{
		
	}
	
	TypeChecker::TypeChecker() = default;
	
	void TypeChecker::run(AbstractSyntaxTree* node) {
		SC_ASSERT(!used, "TypeChecker has been used before");
		used = true;
		doRun(node);
	}
	
	void TypeChecker::doRun(AbstractSyntaxTree* node) {
		doRun(node, node->nodeType());
	}
	
	void TypeChecker::doRun(AbstractSyntaxTree* inNode, NodeType type) {
		switch (type) {
			case NodeType::TranslationUnit: {
				auto* const node = static_cast<TranslationUnit*>(inNode);
				for (auto& decl: node->nodes) {
					doRun(decl.get());
				}
				break;
			}
				
			case NodeType::Block: {
				auto* const node = static_cast<Block*>(inNode);
				for (auto& statement: node->statements) {
					doRun(statement.get());
				}
				break;
			}
			case NodeType::FunctionDeclaration: {
				auto* const node = static_cast<FunctionDeclaration*>(inNode);
				auto const& returnType = typeTable.findByName(node->declReturnTypename.id);
				doRun(node->name.get());
				
				
				node->returnTypeID = returnType.id();
				for (auto& param: node->params) {
					doRun(param.get());
				}
				break;
			}
			case NodeType::FunctionDefinition: {
				auto* const node = static_cast<FunctionDefinition*>(inNode);
				currentFunction = node;
				doRun(node, NodeType::FunctionDeclaration);
				doRun(node->body.get());
				currentFunction = nullptr;
				break;
			}
			case NodeType::VariableDeclaration: {
				auto* const node = static_cast<VariableDeclaration*>(inNode);
				if (node->initExpression == nullptr) {
					if (node->declTypename.empty()) {
						throw TypeError("Expected typename for variable declaration", node->token());
					}
					break;
				}
				doRun(node->initExpression.get());
				if (!node->declTypename.empty()) {
					node->typeID = typeTable.findByName(node->declTypename).id();
					verifyConversion(node->initExpression.get(), node->typeID);
				}
				else {
					node->typeID = node->initExpression->typeID;
				}
				
				break;
			}
			case NodeType::ExpressionStatement: {
				auto* const node = static_cast<ExpressionStatement*>(inNode);
				doRun(node->expression.get());
				break;
			}
			case NodeType::ReturnStatement: {
				auto* const node = static_cast<ReturnStatement*>(inNode);
				doRun(node->expression.get());
				SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
				verifyConversion(node->expression.get(), currentFunction->returnTypeID);
				break;
			}
			case NodeType::IfStatement: {
				auto* const node = static_cast<IfStatement*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), typeTable.Bool());
				doRun(node->ifBlock.get());
				doRun(node->elseBlock.get());
				break;
			}
			case NodeType::WhileStatement: {
				auto* const node = static_cast<WhileStatement*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), typeTable.Bool());
				doRun(node->block.get());
				break;
			}
				
			case NodeType::Identifier: {
				auto* const node = static_cast<Identifier*>(inNode);
				/// TODO: Make a hierarchical table of all identifiers and classify them
				break;
			}
			case NodeType::NumericLiteral: {
				auto* const node = static_cast<NumericLiteral*>(inNode);
				node->typeID = typeTable.Int();
				break;
			}
			case NodeType::StringLiteral: {
				auto* const node = static_cast<StringLiteral*>(inNode);
				node->typeID = typeTable.String();
				break;
			}
				
				/// TODO: A lot of work still need to be done here
			case NodeType::UnaryPrefixExpression: {
				auto* const node = static_cast<UnaryPrefixExpression*>(inNode);
				doRun(node->operand.get());
				break;
			}
				
			case NodeType::BinaryExpression: {
				auto* const node = static_cast<BinaryExpression*>(inNode);
				doRun(node->lhs.get());
				doRun(node->rhs.get());
				node->typeID = verifyBinaryOperation(node);
				break;
			}
			case NodeType::MemberAccess: {
				auto* const node = static_cast<MemberAccess*>(inNode);
				doRun(node->object.get());
				break;
			}
				
			case NodeType::Conditional: {
				auto* const node = static_cast<Conditional*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), typeTable.Bool());
				doRun(node->ifExpr.get());
				doRun(node->elseExpr.get());
				
				break;
			}
				
			case NodeType::FunctionCall: {
				auto* const node = static_cast<FunctionCall*>(inNode);
				doRun(node->object.get());
				for (auto& arg: node->arguments) {
					doRun(arg.get());
				}
				break;
			}
			case NodeType::Subscript: {
				auto* const node = static_cast<Subscript*>(inNode);
				doRun(node->object.get());
				for (auto& arg: node->arguments) {
					doRun(arg.get());
				}
				break;
			}
				
			case NodeType::_count:
				SC_DEBUGFAIL();
		}
	}

	void TypeChecker::verifyConversion(Expression const* from, TypeID to) {
		if (from->typeID != to) {
			throw ImplicitConversionError(typeTable, from->typeID, to, from->token());
		}
	}
	
	TypeID TypeChecker::verifyBinaryOperation(BinaryExpression const* expr) {
		auto doThrow = [&]{
			throw TypeError("Invalid types for operator " + std::string(toString(expr->op)),
							expr->token());
		};
		auto verifySame = [&]{
			if (expr->lhs->typeID != expr->rhs->typeID) {
				doThrow();
			}
		};
		
		switch (expr->op) {
				using enum BinaryOperator;
			case Multiplication: [[fallthrough]];
			case Division:       [[fallthrough]];
			case Remainder:      [[fallthrough]];
			case Addition:       [[fallthrough]];
			case Subtraction:    [[fallthrough]];
			case BitwiseAnd:     [[fallthrough]];
			case BitwiseXOr:     [[fallthrough]];
			case BitwiseOr:
				verifySame();
				return expr->lhs->typeID;
				
			case LeftShift:      [[fallthrough]];
			case RightShift:
				if (expr->lhs->typeID != typeTable.Int()) {
					doThrow();
				}
				if (expr->rhs->typeID != typeTable.Int()) {
					doThrow();
				}
				return expr->lhs->typeID;
				
			case Less:           [[fallthrough]];
			case LessEq:         [[fallthrough]];
			case Greater:        [[fallthrough]];
			case GreaterEq:      [[fallthrough]];
			case Equals:         [[fallthrough]];
			case NotEquals:
				verifySame();
				return typeTable.Bool();
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				if (expr->lhs->typeID != typeTable.Bool()) {
					doThrow();
				}
				if (expr->rhs->typeID != typeTable.Bool()) {
					doThrow();
				}
				return typeTable.Bool();
				
			case Assignment:     [[fallthrough]];
			case AddAssignment:  [[fallthrough]];
			case SubAssignment:  [[fallthrough]];
			case MulAssignment:  [[fallthrough]];
			case DivAssignment:  [[fallthrough]];
			case RemAssignment:  [[fallthrough]];
			case LSAssignment:   [[fallthrough]];
			case RSAssignment:   [[fallthrough]];
			case AndAssignment:  [[fallthrough]];
			case OrAssignment:
				verifySame();
				return typeTable.Void();
				
			case Comma:
				return expr->rhs->typeID;
				
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
}
