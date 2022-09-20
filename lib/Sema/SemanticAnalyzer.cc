#define UTL_DEFER_MACROS

#include "SemanticAnalyzer.h"

#include <sstream>
#include <utl/strcat.hpp>
#include <utl/scope_guard.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Sema/SemanticElements.h"
#include "Sema/SemanticError.h"

namespace scatha::sema {

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
	
	static SymbolID extracted(scatha::ast::Identifier *identifier, const scatha::sema::SymbolTable &symbols) {
		auto const functionSymbolID = symbols.lookupName(identifier->token());
		return functionSymbolID;
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
				
				if (node->scopeKind == Scope::Anonymous) {
					if (currentFunction == nullptr) {
						throw InvalidStatement(node->token(), "Anonymous blocks can only appear at function scope");
					}
					
					node->scopeSymbolID = symbols.addAnonymousSymbol(SymbolCategory::Function);
				}
				
				symbols.pushScope(node->scopeSymbolID);
				utl_defer { symbols.popScope(); };
				for (auto& statement: node->statements) {
					doRun(statement.get());
				}
				return;
			}
			
			case NodeType::FunctionDeclaration: {
				auto* const fnDecl = static_cast<FunctionDeclaration*>(inNode);
				if (auto const sk = symbols.currentScope()->kind();
					sk != Scope::Global && sk != Scope::Namespace && sk != Scope::Struct)
				{
					throw InvalidFunctionDeclaration(fnDecl->token(), symbols.currentScope());
				}
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
				fnDecl->symbolID = func->symbolID();
				fnDecl->functionTypeID = func->typeID();
				return;
			}
				
			case NodeType::FunctionDefinition: {
				auto* const node = static_cast<FunctionDefinition*>(inNode);
				currentFunction = node;
				utl_defer { currentFunction = nullptr; };
				
				doRun(node, NodeType::FunctionDeclaration);

				SC_ASSERT_AUDIT(symbols.currentScope()->findIDByName(node->name()) == node->symbolID, "Just to be sure");
				
				/* Declare parameters to the function scope */ {
					symbols.pushScope(node->symbolID);
					utl_defer { symbols.popScope(); };
					for (auto& param: node->parameters) {
						auto [var, newlyAdded] = symbols.declareVariable(param->token(), param->typeID, true);
						if (!newlyAdded) {
							throw InvalidRedeclaration(param->token(), symbols.currentScope());
						}
						param->symbolID = var->symbolID();
					}
				}
				
				node->body->scopeKind = Scope::Function;
				node->body->scopeSymbolID = node->symbolID;
				doRun(node->body.get());
				
				return;
			}
				
			case NodeType::StructDeclaration: {
				auto* const sDecl = static_cast<StructDeclaration*>(inNode);
				if (auto const sk = symbols.currentScope()->kind();
					sk != Scope::Global && sk != Scope::Namespace && sk != Scope::Struct)
				{
					throw InvalidStructDeclaration(sDecl->token(), symbols.currentScope());
				}
				sDecl->symbolID = symbols.declareType(sDecl->token());
				return;
			}
			
			case NodeType::StructDefinition: {
				auto* const node = static_cast<StructDefinition*>(inNode);
				
				doRun(node, NodeType::StructDeclaration);

				SC_ASSERT_AUDIT(symbols.currentScope()->findIDByName(node->name()) == node->symbolID, "Just to be sure");
				
				node->body->scopeKind = Scope::Struct;
				node->body->scopeSymbolID = node->symbolID;
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
						auto const typeSymbolID = symbols.lookupName(node->declTypename);
						if (!typeSymbolID) {
							throw UseOfUndeclaredIdentifier(node->declTypename);
						}
						if (typeSymbolID.category() != SymbolCategory::Type) {
							throw InvalidStatement(node->declTypename,
												   utl::strcat("\"", node->declTypename, "\" does not name a type"));
						}
						auto& type = symbols.getType(typeSymbolID);
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
					node->symbolID = var->symbolID();
				}
				return;
			}
				
			case NodeType::ExpressionStatement: {
				auto* const node = static_cast<ExpressionStatement*>(inNode);
				if (symbols.currentScope()->kind() != Scope::Function) {
					throw InvalidStatement(node->token(), "Expression statements can only appear at function scope.");
				}
				doRun(node->expression.get());
				return;
			}
				
			case NodeType::ReturnStatement: {
				auto* const node = static_cast<ReturnStatement*>(inNode);
				if (symbols.currentScope()->kind() != Scope::Function) {
					throw InvalidStatement(node->token(), "Return statements can only appear at function scope.");
				}
				doRun(node->expression.get());
				SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
				verifyConversion(node->expression.get(), currentFunction->returnTypeID);
				return;
			}
				
			case NodeType::IfStatement: {
				auto* const node = static_cast<IfStatement*>(inNode);
				if (symbols.currentScope()->kind() != Scope::Function) {
					throw InvalidStatement(node->token(), "If statements can only appear at function scope.");
				}
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), symbols.Bool());
				doRun(node->ifBlock.get());
				if (node->elseBlock != nullptr) {
					doRun(node->elseBlock.get());
				}
				return;
			}
			case NodeType::WhileStatement: {
				auto* const node = static_cast<WhileStatement*>(inNode);
				if (symbols.currentScope()->kind() != Scope::Function) {
					throw InvalidStatement(node->token(), "While statements can only appear at function scope.");
				}
				doRun(node->condition.get());
				verifyConversion(node->condition.get(), symbols.Bool());
				doRun(node->block.get());
				return;
			}
				
			case NodeType::Identifier: {
				auto* const node = static_cast<Identifier*>(inNode);
				auto const symbolID = symbols.lookupName(node->token());
				
				if (!symbolID) {
					throw UseOfUndeclaredIdentifier(node->token());
				}
				
				node->symbolID = symbolID;
				
				if (!(symbolID.category() & (SymbolCategory::Variable | SymbolCategory::Function))) {
					/// TODO: Throw something better here
					throw SemanticError(node->token(), "Invalid use of identifier");
				}
				
				if (symbolID.category() == SymbolCategory::Variable) {
					auto const& var = symbols.getVariable(symbolID);
					node->typeID = var.typeID();
				}
				else if (symbolID.category() == SymbolCategory::Function) {
					auto const& fn = symbols.getFunction(symbolID);
					node->typeID = fn.typeID();
				}
					
				return;
			}
				
			case NodeType::IntegerLiteral: {
				auto* const node = static_cast<IntegerLiteral*>(inNode);
				node->typeID = symbols.Int();
				return;
			}
				
			case NodeType::FloatingPointLiteral: {
				auto* const node = static_cast<FloatingPointLiteral*>(inNode);
				node->typeID = symbols.Float();
				return;
			}
				
			case NodeType::StringLiteral: {
				auto* const node = static_cast<StringLiteral*>(inNode);
				node->typeID = symbols.String();
				return;
			}
				
				/// TODO: A lot of work still needs to be done here
				/// add a way to define and to lookup operators
			case NodeType::UnaryPrefixExpression: {
				auto* const node = static_cast<UnaryPrefixExpression*>(inNode);
				doRun(node->operand.get());
				auto const& operandType = symbols.getType(node->operand->typeID);
				auto doThrow = [&]{
					throw SemanticError(node->token(),
										utl::strcat("Operator \"", toString(node->op), "\" not defined for ", operandType.name()));
				};
				if (!operandType.isBuiltin() || operandType.id() == symbols.String()) {
					doThrow();
				}
				switch (node->op) {
					case ast::UnaryPrefixOperator::Promotion: [[fallthrough]];
					case ast::UnaryPrefixOperator::Negation:
						if (operandType.id() != symbols.Int() ||
							operandType.id() != symbols.Float())
						{
							doThrow();
						}
						break;
					
					case ast::UnaryPrefixOperator::BitwiseNot:
						if (operandType.id() != symbols.Int()) {
							doThrow();
						}
						break;
					
					case ast::UnaryPrefixOperator::LogicalNot:
						if (operandType.id() != symbols.Bool()) {
							doThrow();
						}
						break;
					
					SC_NO_DEFAULT_CASE();
				}
				node->typeID = node->operand->typeID;
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
				
					const scatha::sema::SymbolID functionSymbolID = extracted(identifier, symbols);
					if (functionSymbolID.category() != SymbolCategory::Function) {
						throw; // Look at the assertion above
					}
					auto const& function = symbols.getFunction(functionSymbolID);
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
			throw SemanticError(expr->token(), utl::strcat("Invalid types for operator ", toString(expr->op), ": \"",
														   symbols.getType(expr->lhs->typeID).name(), "\" and \"",
														   symbols.getType(expr->rhs->typeID).name(), "\""));
		};
		
		auto verifySame = [&]{
			if (expr->lhs->typeID != expr->rhs->typeID) {
				doThrow();
			}
		};

		auto verifyAnyOf = [&](TypeID toCheck, std::initializer_list<TypeID> ids) {
			bool result = false;
			for (auto id: ids) {
				if (toCheck == id) { result = true; }
			}
			if (!result) {
				doThrow();
			}
		};
		
		switch (expr->op) {
				using enum BinaryOperator;
			case Multiplication: [[fallthrough]];
			case Division:       [[fallthrough]];
			case Addition:       [[fallthrough]];
			case Subtraction:
				verifySame();
				verifyAnyOf(expr->lhs->typeID, { symbols.Int(), symbols.Float() });
				return expr->lhs->typeID;
				
			case Remainder:
				verifySame();
				verifyAnyOf(expr->lhs->typeID, { symbols.Int() });
				return expr->lhs->typeID;
				
			case BitwiseAnd:     [[fallthrough]];
			case BitwiseXOr:     [[fallthrough]];
			case BitwiseOr:
				verifySame();
				verifyAnyOf(expr->lhs->typeID, { symbols.Int() });
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
				verifyAnyOf(expr->lhs->typeID, { symbols.Int(), symbols.Float() });
				return symbols.Bool();
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				verifySame();
				verifyAnyOf(expr->lhs->typeID, { symbols.Bool() });
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

