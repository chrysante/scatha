#define UTL_DEFER_MACROS

#include "TypeChecker.h"

#include <sstream>
#include <utl/strcat.hpp>
#include <utl/scope_guard.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Common/Type.h"

namespace scatha::ast {

	std::string TypeError::makeString(std::string_view brief, Token const& token, std::string_view message) {
		std::stringstream sstr;
		sstr << brief << " at Line: " << token.sourceLocation.line << " Col: " << token.sourceLocation.column;
		if (!message.empty()) {
			sstr << ": \n" << message;
		}
		return sstr.str();
	}
	
	ImplicitConversionError::ImplicitConversionError(IdentifierTable const& tbl, TypeID from, TypeID to, Token const& token):
		TypeError(utl::strcat("Cannot convert from ", tbl.getType(from).name(), " to ", tbl.getType(to).name()), token)
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
				auto const& returnType = identifiers.findTypeByName(node->declReturnTypename.id);
				node->returnTypeID = returnType.id();
				
				auto [func, newlyAdded] = identifiers.declareFunction(node->name->value);
				SC_ASSERT(newlyAdded, "we dont support forward declarations or overloading just yet");
				
				identifiers.pushScope(node->name->value);
				utl_defer { identifiers.popScope(); };
				utl::small_vector<TypeID> argTypes;
				for (auto& param: node->params) {
					doRun(param.get());
					argTypes.push_back(param->typeID);
				}
				
				// TODO: somehow add the type of the function
				
				break;
			}
				
			case NodeType::FunctionDefinition: {
				auto* const node = static_cast<FunctionDefinition*>(inNode);
				currentFunction = node;
				utl_defer { currentFunction = nullptr; };
				
				doRun(node, NodeType::FunctionDeclaration);
				
				identifiers.pushScope(node->name->value);
				utl_defer { identifiers.popScope(); };
				
				doRun(node->body.get());
				
				break;
			}
				
			case NodeType::VariableDeclaration: {
				auto* const node = static_cast<VariableDeclaration*>(inNode);
				if (node->initExpression == nullptr) {
					if (node->declTypename.empty()) {
						throw TypeError("Expected typename for variable declaration", node->token());
					}
					else {
						auto const typeID = identifiers.lookupName(node->declTypename);
						SC_ASSERT(typeID.category() == NameCategory::Type, "TODO: make this an exception");
						auto& type = identifiers.getType(typeID);
						node->typeID = type.id();
					}
				}
				else {
					doRun(node->initExpression.get());
					if (!node->declTypename.empty()) {
						node->typeID = identifiers.findTypeByName(node->declTypename).id();
						verifyConversion(node->initExpression.get(), node->typeID);
					}
					else {
						node->typeID = node->initExpression->typeID;
					}
				}
				auto [var, newlyAdded] = identifiers.declareVariable(node->name->value, node->typeID, node->isConstant);
				SC_ASSERT(newlyAdded, "we dont support forward declarations just yet"); // TODO: This should throw obviously
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
				verifyConversion(node->condition.get(), identifiers.Bool());
				doRun(node->ifBlock.get());
				doRun(node->elseBlock.get());
				break;
			}
			case NodeType::WhileStatement: {
				auto* const node = static_cast<WhileStatement*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), identifiers.Bool());
				doRun(node->block.get());
				break;
			}
				
			case NodeType::Identifier: {
				auto* const node = static_cast<Identifier*>(inNode);
				
				// TODO: Right now identifiers can only refer to values.
				// This is because Identifier inherits Expression which is a value. If Identifier where to inherit AST and we have an additional class IDExpression which inherits Expression and Identifier, we would have a diamond hierarchy. Do we want that??
				auto const nameID = identifiers.lookupName(node->value, NameCategory::Value);
				auto const& var = identifiers.getVariable(nameID);
				node->typeID = var.typeID();
				
				break;
			}
				
			case NodeType::NumericLiteral: {
				auto* const node = static_cast<NumericLiteral*>(inNode);
				node->typeID = identifiers.Int();
				break;
			}
				
			case NodeType::StringLiteral: {
				auto* const node = static_cast<StringLiteral*>(inNode);
				node->typeID = identifiers.String();
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
				verifyConversion(node->condition.get(), identifiers.Bool());
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
			throw ImplicitConversionError(identifiers, from->typeID, to, from->token());
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
				if (expr->lhs->typeID != identifiers.Int()) {
					doThrow();
				}
				if (expr->rhs->typeID != identifiers.Int()) {
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
				return identifiers.Bool();
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				if (expr->lhs->typeID != identifiers.Bool()) {
					doThrow();
				}
				if (expr->rhs->typeID != identifiers.Bool()) {
					doThrow();
				}
				return identifiers.Bool();
				
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
				return identifiers.Void();
				
			case Comma:
				return expr->rhs->typeID;
				
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
}
