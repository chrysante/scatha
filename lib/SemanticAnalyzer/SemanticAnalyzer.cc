#define UTL_DEFER_MACROS

#include "SemanticAnalyzer.h"

#include <sstream>
#include <utl/strcat.hpp>
#include <utl/scope_guard.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "SemanticAnalyzer/SemanticElements.h"
#include "SemanticAnalyzer/SemanticError.h"

namespace scatha::sem {

	using namespace ast;

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
				auto const& returnType = symbols.findTypeByName(fnDecl->declReturnTypename);
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
				
				auto [func, newlyAdded] = symbols.declareFunction(fnDecl->token(), returnType.id(), argTypes);
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
					symbols.declareVariable(param->token(), param->typeID, true);
				}
				
				doRun(node->body.get());
				
				return;
			}
				
			case NodeType::VariableDeclaration: {
				auto* const node = static_cast<VariableDeclaration*>(inNode);
				if (node->initExpression == nullptr) {
					if (node->declTypename.empty()) {
						throw InvalidStatement(node->token(), "Expected initializing expression of explicit typename specifier in variable declaration");
					}
					else {
						auto const typeNameID = symbols.lookupName(node->declTypename);
						if (!typeNameID) {
							throw UseOfUndeclaredIdentifier(node->declTypename);
						}
						if (typeNameID.category() != NameCategory::Type) {
							throw InvalidStatement(node->declTypename,
												   utl::strcat("\"", node->declTypename, "\" does not name a type"));
						}
						auto& type = symbols.getType(typeNameID);
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
					auto [var, newlyAdded] = symbols.declareVariable(node->token(), node->typeID, node->isConstant);
					if (!newlyAdded) {
						throw InvalidRedeclaration(node->token(), symbols.currentScope());
					}
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
				auto const nameID = symbols.lookupName(node->token());
				
				if (!nameID) {
					throw UseOfUndeclaredIdentifier(node->token());
				}
				
				if (!(nameID.category() & (NameCategory::Variable | NameCategory::Function))) {
					/// TODO: Throw something better here
					throw SemanticError(node->token(), "Invalid use of identifier");
				}
				
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
				
				{
					// Temporary fix to allow function calls.
					// This must be changed to allow for overloading of operator() and function objects.
					// Getting the type of the expression is not enough (if it's a function) as we won't know which function to call.
					
					auto* const identifier = dynamic_cast<Identifier*>(node->object.get());
					SC_ASSERT(identifier != nullptr, "Called object must be an identifier. We do not yet support calling arbitrary expressions.");
				
					auto const functionNameID = symbols.lookupName(identifier->token());
					if (functionNameID.category() != NameCategory::Function) {
						throw; // Look at the assertion above
					}
					auto const& function = symbols.getFunction(functionNameID);
					auto const& functionType = symbols.getType(function.typeID());
					
					verifyFunctionCallExpression(node, functionType, node->arguments);
					
					node->typeID = functionType.returnType();
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

	void SemanticAnalyzer::verifyConversion(Expression const* from, TypeID to) const {
		if (from->typeID != to) {
			throwBadTypeConversion(from->token(), from->typeID, to);
		}
	}
	
	TypeID SemanticAnalyzer::verifyBinaryOperation(BinaryExpression const* expr) const {
		auto doThrow = [&]{
			/// TODO: Think of somethin better here
			/// probably think of some way of how to lookup and define operators
			throw SemanticError(expr->token(), "Invalid types for operator " + std::string(toString(expr->op)));
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
	
	void SemanticAnalyzer::verifyFunctionCallExpression(FunctionCall const* expr, TypeEx const& fnType, std::span<UniquePtr<Expression> const> arguments) const {
		SC_ASSERT(fnType.isFunctionType(), "fnType is not a function type");
		if (fnType.argumentCount() != arguments.size()) {
			throw BadFunctionCall(expr->object->token(), BadFunctionCall::WrongArgumentCount);
		}
		for (size_t i = 0; i < arguments.size(); ++i) {
			verifyConversion(arguments[i].get(), fnType.argumentType(i));
		}
	}
	
	void SemanticAnalyzer::throwBadTypeConversion(Token const& token, TypeID from, TypeID to) const {
		throw BadTypeConversion(token, symbols.getType(from), symbols.getType(to));
	}
	
}

