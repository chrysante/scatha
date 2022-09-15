#define UTL_DEFER_MACROS

#include "SemanticAnalyzer.h"

#include <sstream>
#include <utl/strcat.hpp>
#include <utl/scope_guard.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "SemanticAnalyzer/SemanticElements.h"

namespace scatha::sem {

	using namespace ast;
	
	std::string TypeError::makeString(std::string_view brief, Token const& token, std::string_view message) {
		std::stringstream sstr;
		sstr << brief << " at Line: " << token.sourceLocation.line << " Col: " << token.sourceLocation.column;
		if (!message.empty()) {
			sstr << ": \n" << message;
		}
		return sstr.str();
	}
	
	ImplicitConversionError::ImplicitConversionError(SymbolTable const& tbl, TypeID from, TypeID to, Token const& token):
		TypeError(utl::strcat("Cannot convert from ", tbl.getType(from).name(), " to ", tbl.getType(to).name()), token)
	{
		
	}
	
	SemanticAnalyzer::SemanticAnalyzer() = default;
	
	void SemanticAnalyzer::run(AbstractSyntaxTree* node) {
		SC_ASSERT(!used, "SemanticAnalyzer has been used before");
		used = true;
		doRun(node);
	}
	
	void SemanticAnalyzer::doRun(AbstractSyntaxTree* node) {
		doRun(node, node->nodeType());
	}
	
	void SemanticAnalyzer::doRun(AbstractSyntaxTree* inNode, NodeType type) {
		switch (type) {
			case NodeType::TranslationUnit: {
				auto* const tu = static_cast<TranslationUnit*>(inNode);
				for (auto& decl: tu->declarations) {
					doRun(decl.get());
				}
				return;
			}
				
			case NodeType::Block: {
				auto* const node = static_cast<Block*>(inNode);
				for (auto& statement: node->statements) {
					doRun(statement.get());
				}
				return;
			}
			
			case NodeType::FunctionDeclaration: {
				auto* const fnDecl = static_cast<FunctionDeclaration*>(inNode);
				auto const& returnType = symbols.findTypeByName(fnDecl->declReturnTypename.id);
				fnDecl->returnTypeID = returnType.id();
				
				/*
				 * No need to push the scope here, since function parameter declarations don't declare variables
				 * in the current scope. This will be done be in the function definition case.
				 */
				utl::small_vector<TypeID> argTypes;
				for (auto& param: fnDecl->parameters) {
					doRun(param.get());
					argTypes.push_back(param->typeID);
				}
				
				auto [func, newlyAdded] = symbols.declareFunction(fnDecl->name(), returnType.id(), argTypes);
				fnDecl->nameID = func->nameID();
				
				return;
			}
				
			case NodeType::FunctionDefinition: {
				auto* const node = static_cast<FunctionDefinition*>(inNode);
				currentFunction = node;
				utl_defer { currentFunction = nullptr; };
				
				doRun(node, NodeType::FunctionDeclaration);

				SC_ASSERT_AUDIT(symbols.currentScope()->findIDByName(node->name()) == node->nameID, "Just to be sure");
				symbols.pushScope(node->nameID);
				utl_defer { symbols.popScope(); };
				
				// Now we begin declaring to the function scope
				for (auto& param: node->parameters) {
					symbols.declareVariable(param->name(), param->typeID, true);
				}
				
				doRun(node->body.get());
				
				return;
			}
				
			case NodeType::VariableDeclaration: {
				auto* const node = static_cast<VariableDeclaration*>(inNode);
				if (node->initExpression == nullptr) {
					if (node->declTypename.empty()) {
						throw TypeError("Expected typename for variable declaration", node->token());
					}
					else {
						auto const typeID = symbols.lookupName(node->declTypename);
						SC_ASSERT(typeID.category() == NameCategory::Type, "TODO: make this an exception");
						auto& type = symbols.getType(typeID);
						node->typeID = type.id();
					}
				}
				else {
					doRun(node->initExpression.get());
					if (!node->declTypename.empty()) {
						node->typeID = symbols.findTypeByName(node->declTypename).id();
						verifyConversion(node->initExpression.get(), node->typeID);
					}
					else {
						node->typeID = node->initExpression->typeID;
					}
				}
				if (!node->isFunctionParameter) /* Function parameters will be declared by the FunctionDefinition case */ {
					auto [var, newlyAdded] = symbols.declareVariable(node->name(), node->typeID, node->isConstant);
					
					SC_ASSERT(newlyAdded, "we dont support multiple declarations just yet"); // TODO: This should throw obviously
					node->nameID = var->nameID();
				}
				return;
			}
				
			case NodeType::ExpressionStatement: {
				auto* const node = static_cast<ExpressionStatement*>(inNode);
				doRun(node->expression.get());
				return;
			}
				
			case NodeType::ReturnStatement: {
				auto* const node = static_cast<ReturnStatement*>(inNode);
				doRun(node->expression.get());
				SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
				verifyConversion(node->expression.get(), currentFunction->returnTypeID);
				return;
			}
				
			case NodeType::IfStatement: {
				auto* const node = static_cast<IfStatement*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), symbols.Bool());
				doRun(node->ifBlock.get());
				doRun(node->elseBlock.get());
				return;
			}
			case NodeType::WhileStatement: {
				auto* const node = static_cast<WhileStatement*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), symbols.Bool());
				doRun(node->block.get());
				return;
			}
				
			case NodeType::Identifier: {
				auto* const node = static_cast<Identifier*>(inNode);
				
				// TODO: Right now symbols can only refer to values.
				// This is because Identifier inherits Expression which is a value. If Identifier where to inherit AST and we have an additional class IDExpression which inherits Expression and Identifier, we would have a diamond hierarchy. Do we want that??
				
				
				auto const nameID = symbols.lookupName(node->value, NameCategory::Variable | NameCategory::Function);
				if (nameID.category() == NameCategory::Variable) {
					auto const& var = symbols.getVariable(nameID);
					node->typeID = var.typeID();
				}
				else if (nameID.category() == NameCategory::Function) {
					auto const& fn = symbols.getFunction(nameID);
					node->typeID = fn.typeID();
				}
				
				
				return;
			}
				
			case NodeType::IntegerLiteral: {
				auto* const node = static_cast<IntegerLiteral*>(inNode);
				node->typeID = symbols.Int();
				return;
			}
				
			case NodeType::StringLiteral: {
				auto* const node = static_cast<StringLiteral*>(inNode);
				node->typeID = symbols.String();
				return;
			}
				
				/// TODO: A lot of work still need to be done here
			case NodeType::UnaryPrefixExpression: {
				auto* const node = static_cast<UnaryPrefixExpression*>(inNode);
				doRun(node->operand.get());
				return;
			}
				
			case NodeType::BinaryExpression: {
				auto* const node = static_cast<BinaryExpression*>(inNode);
				doRun(node->lhs.get());
				doRun(node->rhs.get());
				node->typeID = verifyBinaryOperation(node);
				return;
			}
				
			case NodeType::MemberAccess: {
				auto* const node = static_cast<MemberAccess*>(inNode);
				doRun(node->object.get());
				return;
			}
				
			case NodeType::Conditional: {
				auto* const node = static_cast<Conditional*>(inNode);
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), symbols.Bool());
				doRun(node->ifExpr.get());
				doRun(node->elseExpr.get());
				
				return;
			}
				
			case NodeType::FunctionCall: {
				auto* const node = static_cast<FunctionCall*>(inNode);
				doRun(node->object.get());
				for (auto& arg: node->arguments) {
					doRun(arg.get());
				}
				return;
			}
			case NodeType::Subscript: {
				auto* const node = static_cast<Subscript*>(inNode);
				doRun(node->object.get());
				for (auto& arg: node->arguments) {
					doRun(arg.get());
				}
				return;
			}
				
			case NodeType::_count:
				SC_DEBUGFAIL();
		}
	}

	void SemanticAnalyzer::verifyConversion(Expression const* from, TypeID to) {
		if (from->typeID != to) {
			throw ImplicitConversionError(symbols, from->typeID, to, from->token());
		}
	}
	
	TypeID SemanticAnalyzer::verifyBinaryOperation(BinaryExpression const* expr) {
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
				if (expr->lhs->typeID != symbols.Int()) {
					doThrow();
				}
				if (expr->rhs->typeID != symbols.Int()) {
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
				return symbols.Bool();
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				if (expr->lhs->typeID != symbols.Bool()) {
					doThrow();
				}
				if (expr->rhs->typeID != symbols.Bool()) {
					doThrow();
				}
				return symbols.Bool();
				
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
				return symbols.Void();
				
			case Comma:
				return expr->rhs->typeID;
				
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
}

