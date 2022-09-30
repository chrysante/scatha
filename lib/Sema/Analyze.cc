#define UTL_DEFER_MACROS

#include "Sema/Analyze.h"

#include <sstream>
#include <utl/strcat.hpp>
#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Common.h"
#include "AST/Expression.h"
#include "AST/Visit.h"
#include "Basic/Basic.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {

	using namespace ast;
	
	namespace {
		struct Context {
			void prepass(AbstractSyntaxTree&);
			
			void analyze(AbstractSyntaxTree&);
			
			void verifyConversion(ast::Expression const& from, TypeID to) const;
			TypeID verifyBinaryOperation(ast::BinaryExpression const&) const;
			void verifyFunctionCallExpression(ast::FunctionCall const&, FunctionSignature const& fnType, std::span<ast::UniquePtr<ast::Expression> const> arguments) const;
			[[noreturn]] void throwBadTypeConversion(Token const&, TypeID from, TypeID to) const;
			
			SymbolTable& sym;
			ast::FunctionDefinition* currentFunction = nullptr;
		};
	} // namespace
	
	SymbolTable analyze(AbstractSyntaxTree* root) {
		SymbolTable sym;
		Context ctx{ sym };
		ctx.analyze(*root);
		return sym;
	}
	
#if 0
//	void Context::prepass(AbstractSyntaxTree& node) {
//		visit(node, utl::visitor{
//			[&](FunctionDefinition& fn) {
//				if (auto const sk = sym.currentScope()->kind();
//					sk != Scope::Global &&
//					sk != Scope::Namespace &&
//					sk != Scope::Struct)
//				{
//					/*
//					 * Function defintion is only allowed in the global scope, at namespace scope and structure scope
//					 */
//					throw InvalidFunctionDeclaration(fn.token(), sym.currentScope());
//				}
//				auto const& returnType = sym.findTypeByName(fn.declReturnTypename);
//				fn.returnTypeID = returnType.id();
//
//				/*
//				 * No need to push the scope here, since function parameter declarations don't declare variables
//				 * in the current scope. This will be done be in the function definition case.
//				 */
//				utl::small_vector<TypeID> argTypes;
//				for (auto& param: fn.parameters) {
//					analyze(*param);
//					argTypes.push_back(param->typeID);
//				}
//
//				auto [func, newlyAdded] = sym.declareFunction(fn.token(), returnType.id(), argTypes);
//				fn.symbolID = func->symbolID();
//				fn.functionTypeID = func->typeID();
//
//				currentFunction = &fn;
//				utl_defer { currentFunction = nullptr; };
//
//				/* Declare parameters to the function scope */ {
//					sym.pushScope(fn.symbolID);
//					utl_defer { sym.popScope(); };
//					for (auto& param: fn.parameters) {
//						auto [var, newlyAdded] = sym.declareVariable(param->token(), param->typeID, true);
//						if (!newlyAdded) {
//							throw InvalidRedeclaration(param->token(), sym.currentScope());
//						}
//						param->symbolID = var->symbolID();
//					}
//				}
//
//				fn.body->scopeKind = Scope::Function;
//				fn.body->scopeSymbolID = fn.symbolID;
//				analyze(*fn.body);
//			},
//			[&](StructDefinition& s) {
//				if (auto const sk = sym.currentScope()->kind();
//					sk != Scope::Global && sk != Scope::Namespace && sk != Scope::Struct)
//				{
//					throw InvalidStructDeclaration(s.token(), sym.currentScope());
//				}
//				s.symbolID = sym.declareType(s.token());
//
//				s.body->scopeKind = Scope::Struct;
//				s.body->scopeSymbolID = s.symbolID;
//				analyze(*s.body);
//			},
//			[&](VariableDeclaration& var) {
//				if (var.initExpression == nullptr) {
//					if (var.declTypename.empty()) {
//						throw InvalidStatement(var.token(), "Expected initializing expression of explicit typename specifier in variable declaration");
//					}
//					else {
//						// Get TtypeID from declared typename
//						auto const typeSymbolID = sym.lookup(var.declTypename);
//						if (!typeSymbolID) {
//							throw UseOfUndeclaredIdentifier(var.declTypename);
//						}
//						if (typeSymbolID.category() != SymbolCategory::Type) {
//							throw InvalidStatement(var.declTypename,
//												   utl::strcat("\"", var.declTypename, "\" does not name a type"));
//						}
//						auto& type = sym.getType(typeSymbolID);
//						var.typeID = type.id();
//					}
//				}
//				else {
//					analyze(*var.initExpression);
//					if (!var.declTypename.empty()) {
//						var.typeID = sym.findTypeByName(var.declTypename).id();
//						verifyConversion(*var.initExpression, var.typeID);
//					}
//					else {
//						var.typeID = var.initExpression->typeID;
//					}
//				}
//				if (var.isFunctionParameter) {
//					// Function parameters will be declared by the FunctionDefinition case
//					return;
//				}
//				/*
//				 * Declare the variable.
//				 */
//				auto [varObj, newlyAdded] = sym.declareVariable(var.token(), var.typeID, var.isConstant);
//				if (!newlyAdded) {
//					throw InvalidRedeclaration(var.token(), sym.currentScope());
//				}
//				var.symbolID = varObj->symbolID();
//			},
//			[&](ExpressionStatement& es) {
//				if (sym.currentScope()->kind() != Scope::Function) {
//					throw InvalidStatement(es.token(), "Expression statements can only appear at function scope.");
//				}
//				analyze(*es.expression);
//			},
//			[&](ReturnStatement& rs) {
//				if (sym.currentScope()->kind() != Scope::Function) {
//					throw InvalidStatement(rs.token(), "Return statements can only appear at function scope.");
//				}
//				analyze(*rs.expression);
//				SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
//				verifyConversion(*rs.expression, currentFunction->returnTypeID);
//			},
//			[&](IfStatement& is) {
//				if (sym.currentScope()->kind() != Scope::Function) {
//					throw InvalidStatement(is.token(), "If statements can only appear at function scope.");
//				}
//				analyze(*is.condition);
//				verifyConversion(*is.condition, sym.Bool());
//				analyze(*is.ifBlock);
//				if (is.elseBlock != nullptr) {
//					analyze(*is.elseBlock);
//				}
//			},
//			[&](WhileStatement& ws) {
//				if (sym.currentScope()->kind() != Scope::Function) {
//					throw InvalidStatement(ws.token(), "While statements can only appear at function scope.");
//				}
//				analyze(*ws.condition);
//				verifyConversion(*ws.condition, sym.Bool());
//				analyze(*ws.block);
//			},
//			[&](Identifier& i) {
//				auto const symbolID = sym.lookup(i.token());
//
//				if (!symbolID) {
//					throw UseOfUndeclaredIdentifier(i.token());
//				}
//
//				i.symbolID = symbolID;
//
//				if (!(symbolID.category() & (SymbolCategory::Variable | SymbolCategory::Function))) {
//					/// TODO: Throw something better here
//					throw SemanticIssue(i.token(), "Invalid use of identifier");
//				}
//
//				if (symbolID.category() == SymbolCategory::Variable) {
//					auto const& var = sym.getVariable(symbolID);
//					i.typeID = var.typeID();
//				}
//				else if (symbolID.category() == SymbolCategory::Function) {
//					auto const& fn = sym.getFunction(symbolID);
//					i.typeID = fn.typeID();
//				}
//			},
//			[&](IntegerLiteral& l) {
//				l.typeID = sym.Int();
//			},
//			[&](BooleanLiteral& l) {
//				l.typeID = sym.Bool();
//			},
//			[&](FloatingPointLiteral& l) {
//				l.typeID = sym.Float();
//			},
//			[&](StringLiteral& l) {
//				l.typeID = sym.String();
//			},
//			[&](UnaryPrefixExpression& u) {
//				analyze(*u.operand);
//				auto const& operandType = sym.getType(u.operand->typeID);
//				auto doThrow = [&]{
//					throw SemanticIssue(u.token(),
//										utl::strcat("Operator \"", toString(u.op), "\" not defined for ", operandType.name()));
//				};
//				if (!operandType.isBuiltin() || operandType.id() == sym.String()) {
//					doThrow();
//				}
//				switch (u.op) {
//					case ast::UnaryPrefixOperator::Promotion: [[fallthrough]];
//					case ast::UnaryPrefixOperator::Negation:
//						if (operandType.id() != sym.Int() &&
//							operandType.id() != sym.Float())
//						{
//							doThrow();
//						}
//						break;
//
//					case ast::UnaryPrefixOperator::BitwiseNot:
//						if (operandType.id() != sym.Int()) {
//							doThrow();
//						}
//						break;
//
//					case ast::UnaryPrefixOperator::LogicalNot:
//						if (operandType.id() != sym.Bool()) {
//							doThrow();
//						}
//						break;
//
//					SC_NO_DEFAULT_CASE();
//				}
//				u.typeID = u.operand->typeID;
//			},
//			[&](BinaryExpression& b) {
//				analyze(*b.lhs);
//				analyze(*b.rhs);
//				b.typeID = verifyBinaryOperation(b);
//			},
//			[&](MemberAccess& ma) {
//				analyze(*ma.object);
//			},
//			[&](Conditional& c) {
//				analyze(*c.condition);
//				verifyConversion(*c.condition, sym.Bool());
//				analyze(*c.ifExpr);
//				analyze(*c.elseExpr);
//			},
//			[&](FunctionCall& fc) {
//				analyze(*fc.object);
//				for (auto& arg: fc.arguments) {
//					analyze(*arg);
//				}
//
//				// Temporary fix to allow function calls.
//				// This must be changed to allow for overloading of operator() and function objects.
//				// Getting the type of the expression is not enough (if it's a function) as we won't know which function to call.
//				SC_ASSERT(fc.object->nodeType() == ast::NodeType::Identifier,
//						  "Called object must be an identifier. We do not yet support calling arbitrary expressions.");
//				auto& identifier = static_cast<Identifier&>(*fc.object);
//
//				scatha::sema::SymbolID const functionSymbolID = sym.lookup(identifier.token().id);
//				if (functionSymbolID.category() != SymbolCategory::Function) {
//					SC_ASSERT(false, "Look at the assertion above");
//				}
//				auto const& function = sym.getFunction(functionSymbolID);
//				auto const& functionType = sym.getType(function.typeID());
//
//				verifyFunctionCallExpression(fc, functionType, fc.arguments);
//
//				fc.typeID = functionType.returnType();
//			},
//			[&](Subscript& s) {
//				analyze(*s.object);
//				for (auto& arg: s.arguments) {
//					analyze(*arg);
//				}
//			},
//
//		});
//	}
#endif
	void Context::analyze(AbstractSyntaxTree& node) {
		visit(node, utl::visitor{
			[&](TranslationUnit& tu) {
				for (auto& decl: tu.declarations) {
					analyze(*decl);
				}
			},
			[&](Block& block) {
				if (block.scopeKind == ScopeKind::Anonymous) {
					if (currentFunction == nullptr) {
						throw InvalidStatement(block.token(), "Anonymous blocks can only appear at function scope");
					}
					block.scopeSymbolID = sym.addAnonymousScope().symbolID();
				}
				sym.pushScope(block.scopeSymbolID);
				utl_defer { sym.popScope(); };
				for (auto& statement: block.statements) {
					analyze(*statement);
				}
			},
			[&](FunctionDefinition& fn) {
				if (auto const sk = sym.currentScope().kind();
					sk != ScopeKind::Global &&
					sk != ScopeKind::Namespace &&
					sk != ScopeKind::Object)
				{
					/*
					 * Function defintion is only allowed in the global scope, at namespace scope and structure scope
					 */
					throw InvalidFunctionDeclaration(fn.token(), sym.currentScope());
				}
				auto const& returnType = *sym.lookupObjectType(fn.declReturnTypename);
				fn.returnTypeID = returnType.symbolID();
				
				/*
				 * No need to push the scope here, since function parameter declarations don't declare variables
				 * in the current scope. This will be done be in the function definition case.
				 */
				utl::small_vector<TypeID> argTypes;
				for (auto& param: fn.parameters) {
					analyze(*param);
					argTypes.push_back(param->typeID);
				}
				
				Expected const func = sym.addFunction(fn.token(), FunctionSignature(argTypes, returnType.symbolID()));
				if (!func.hasValue()) {
					throw InvalidRedeclaration(fn.token(), sym.currentScope());
				}
				fn.symbolID = func->symbolID();
				fn.functionTypeID = func->typeID();
				
				currentFunction = &fn;
				utl_defer { currentFunction = nullptr; };
				
				/* Declare parameters to the function scope */ {
					sym.pushScope(fn.symbolID);
					utl_defer { sym.popScope(); };
					for (auto& param: fn.parameters) {
						auto const var = sym.addVariable(param->token(), param->typeID, true);
						if (!var) {
#warning maybe rethrow?
							throw InvalidRedeclaration(param->token(), sym.currentScope());
						}
						param->symbolID = var->symbolID();
					}
				}
				
				fn.body->scopeKind = ScopeKind::Function;
				fn.body->scopeSymbolID = fn.symbolID;
				analyze(*fn.body);
			},
			[&](StructDefinition& s) {
				if (auto const sk = sym.currentScope().kind();
					sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object)
				{
					throw InvalidStructDeclaration(s.token(), sym.currentScope());
				}
				auto const obj = sym.addObjectType(s.token());
				if (!obj) {
					throw InvalidRedeclaration(s.token(), sym.currentScope());
				}
				s.symbolID = obj->symbolID();
				s.body->scopeKind = ScopeKind::Object;
				s.body->scopeSymbolID = s.symbolID;
				analyze(*s.body);
			},
			[&](VariableDeclaration& var) {
				if (var.initExpression == nullptr) {
					if (var.declTypename.empty()) {
						throw InvalidStatement(var.token(), "Expected initializing expression of explicit typename specifier in variable declaration");
					}
					else {
						// Get TtypeID from declared typename
						auto const typeSymbolID = sym.lookup(var.declTypename);
						if (!typeSymbolID) {
							throw UseOfUndeclaredIdentifier(var.declTypename);
						}
						if (!sym.is(typeSymbolID, SymbolCategory::ObjectType)) {
							throw InvalidStatement(var.declTypename,
												   utl::strcat("\"", var.declTypename, "\" does not name a type"));
						}
						auto& type = sym.getObjectType(typeSymbolID);
						var.typeID = type.symbolID();
					}
				}
				else {
					analyze(*var.initExpression);
					if (!var.declTypename.empty()) {
						auto const* type = sym.lookupObjectType(var.declTypename);
						var.typeID = type->symbolID();
						verifyConversion(*var.initExpression, var.typeID);
					}
					else {
						var.typeID = var.initExpression->typeID;
					}
				}
				if (var.isFunctionParameter) {
					// Function parameters will be declared by the FunctionDefinition case
					return;
				}
				/*
				 * Declare the variable.
				 */
				auto varObj = sym.addVariable(var.token(), var.typeID, var.isConstant);
				if (!varObj) {
					throw InvalidRedeclaration(var.token(), sym.currentScope());
				}
				var.symbolID = varObj->symbolID();
			},
			[&](ExpressionStatement& es) {
				if (sym.currentScope().kind() != ScopeKind::Function) {
					throw InvalidStatement(es.token(), "Expression statements can only appear at function scope.");
				}
				analyze(*es.expression);
			},
			[&](ReturnStatement& rs) {
				if (sym.currentScope().kind() != ScopeKind::Function) {
					throw InvalidStatement(rs.token(), "Return statements can only appear at function scope.");
				}
				analyze(*rs.expression);
				SC_ASSERT(currentFunction != nullptr, "This should have been set by case FunctionDefinition");
				verifyConversion(*rs.expression, currentFunction->returnTypeID);
			},
			[&](IfStatement& is) {
				if (sym.currentScope().kind() != ScopeKind::Function) {
					throw InvalidStatement(is.token(), "If statements can only appear at function scope.");
				}
				analyze(*is.condition);
				verifyConversion(*is.condition, sym.Bool());
				analyze(*is.ifBlock);
				if (is.elseBlock != nullptr) {
					analyze(*is.elseBlock);
				}
			},
			[&](WhileStatement& ws) {
				if (sym.currentScope().kind() != ScopeKind::Function) {
					throw InvalidStatement(ws.token(), "While statements can only appear at function scope.");
				}
				analyze(*ws.condition);
				verifyConversion(*ws.condition, sym.Bool());
				analyze(*ws.block);
			},
			[&](Identifier& i) {
				auto const symbolID = sym.lookup(i.token());
				
				if (!symbolID) {
					throw UseOfUndeclaredIdentifier(i.token());
				}
				
				i.symbolID = symbolID;
				
				if (!sym.is(symbolID, SymbolCategory::Variable | SymbolCategory::OverloadSet)) {
					/// TODO: Throw something better here
					throw SemanticIssue(i.token(), "Invalid use of identifier");
				}
				
				if (sym.is(symbolID, SymbolCategory::Variable)) {
					auto const& var = sym.getVariable(symbolID);
					i.typeID = var.typeID();
				}
				else if (sym.is(symbolID, SymbolCategory::Function)) {
					auto const& fn = sym.getFunction(symbolID);
					i.typeID = fn.typeID();
				}
			},
			[&](IntegerLiteral& l) {
				l.typeID = sym.Int();
			},
			[&](BooleanLiteral& l) {
				l.typeID = sym.Bool();
			},
			[&](FloatingPointLiteral& l) {
				l.typeID = sym.Float();
			},
			[&](StringLiteral& l) {
				l.typeID = sym.String();
			},
			[&](UnaryPrefixExpression& u) {
				analyze(*u.operand);
				auto const& operandType = sym.getObjectType(u.operand->typeID);
				auto doThrow = [&]{
					throw SemanticIssue(u.token(),
										utl::strcat("Operator \"", toString(u.op), "\" not defined for ", operandType.name()));
				};
				if (!operandType.isBuiltin() || operandType.symbolID() == sym.String()) {
					doThrow();
				}
				switch (u.op) {
					case ast::UnaryPrefixOperator::Promotion: [[fallthrough]];
					case ast::UnaryPrefixOperator::Negation:
						if (operandType.symbolID() != sym.Int() &&
							operandType.symbolID() != sym.Float())
						{
							doThrow();
						}
						break;
					
					case ast::UnaryPrefixOperator::BitwiseNot:
						if (operandType.symbolID() != sym.Int()) {
							doThrow();
						}
						break;
					
					case ast::UnaryPrefixOperator::LogicalNot:
						if (operandType.symbolID() != sym.Bool()) {
							doThrow();
						}
						break;
					
					SC_NO_DEFAULT_CASE();
				}
				u.typeID = u.operand->typeID;
			},
			[&](BinaryExpression& b) {
				analyze(*b.lhs);
				analyze(*b.rhs);
				b.typeID = verifyBinaryOperation(b);
			},
			[&](MemberAccess& ma) {
				analyze(*ma.object);
			},
			[&](Conditional& c) {
				analyze(*c.condition);
				verifyConversion(*c.condition, sym.Bool());
				analyze(*c.ifExpr);
				analyze(*c.elseExpr);
			},
			[&](FunctionCall& fc) {
				analyze(*fc.object);
				utl::small_vector<TypeID> argTypes;
				argTypes.reserve(fc.arguments.size());
				for (auto& arg: fc.arguments) {
					analyze(*arg);
					argTypes.push_back(arg->typeID);
				}
					
				// Temporary fix to allow function calls.
				// This must be changed to allow for overloading of operator() and function objects.
				// Getting the type of the expression is not enough (if it's a function) as we won't know which function to call.
				SC_ASSERT(fc.object->nodeType() == ast::NodeType::Identifier,
						  "Called object must be an identifier. We do not yet support calling arbitrary expressions.");
				auto& identifier = static_cast<Identifier&>(*fc.object);
				
				scatha::sema::SymbolID const overloadSetSymbolID = sym.lookup(identifier.token());
				if (!sym.is(overloadSetSymbolID, SymbolCategory::OverloadSet)) {
					SC_ASSERT(false, "Look at the assertion above");
				}
				auto const& overloadSet = sym.getOverloadSet(overloadSetSymbolID);
				
				auto const* functionPtr = overloadSet.find(argTypes);
				if (!functionPtr) {
					throw BadFunctionCall(fc.token(), BadFunctionCall::NoMatchingFunction);
				}
				auto const& function = *functionPtr;
				verifyFunctionCallExpression(fc, function.signature(), fc.arguments);
				identifier.symbolID = function.symbolID();
				identifier.typeID = function.typeID();
				fc.typeID = function.signature().returnTypeID();
			},
			[&](Subscript& s) {
				analyze(*s.object);
				for (auto& arg: s.arguments) {
					analyze(*arg);
				}
			},
			
		});
	}
	
	void Context::verifyConversion(Expression const& from, TypeID to) const {
		if (from.typeID != to) {
			throwBadTypeConversion(from.token(), from.typeID, to);
		}
	}
	
	TypeID Context::verifyBinaryOperation(BinaryExpression const& expr) const {
		auto doThrow = [&]{
			/// TODO: Think of somethin better here
			/// probably think of some way of how to lookup and define operators
			throw SemanticIssue(expr.token(), utl::strcat("Invalid types for operator ", expr.op, ": \"",
														  sym.getObjectType(expr.lhs->typeID).name(), "\" and \"",
														  sym.getObjectType(expr.rhs->typeID).name(), "\""));
		};
		
		auto verifySame = [&]{
			if (expr.lhs->typeID != expr.rhs->typeID) {
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
		
		switch (expr.op) {
				using enum BinaryOperator;
			case Multiplication: [[fallthrough]];
			case Division:       [[fallthrough]];
			case Addition:       [[fallthrough]];
			case Subtraction:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float() });
				return expr.lhs->typeID;
				
			case Remainder:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Int() });
				return expr.lhs->typeID;
				
			case BitwiseAnd:     [[fallthrough]];
			case BitwiseXOr:     [[fallthrough]];
			case BitwiseOr:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Int() });
				return expr.lhs->typeID;
				
			case LeftShift:      [[fallthrough]];
			case RightShift:
				if (expr.lhs->typeID != sym.Int()) {
					doThrow();
				}
				if (expr.rhs->typeID != sym.Int()) {
					doThrow();
				}
				return expr.lhs->typeID;
				
			case Less:           [[fallthrough]];
			case LessEq:         [[fallthrough]];
			case Greater:        [[fallthrough]];
			case GreaterEq:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float() });
				return sym.Bool();
			case Equals:         [[fallthrough]];
			case NotEquals:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float(), sym.Bool() });
				return sym.Bool();
				
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				verifySame();
				verifyAnyOf(expr.lhs->typeID, { sym.Bool() });
				return sym.Bool();
				
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
				return sym.Void();
				
			case Comma:
				return expr.rhs->typeID;
				
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
	void Context::verifyFunctionCallExpression(FunctionCall const& expr, FunctionSignature const& fnType, std::span<UniquePtr<Expression> const> arguments) const {
		if (fnType.argumentCount() != arguments.size()) {
			throw BadFunctionCall(expr.object->token(), BadFunctionCall::WrongArgumentCount);
		}
		for (size_t i = 0; i < arguments.size(); ++i) {
			verifyConversion(*arguments[i], fnType.argumentTypeID(i));
		}
	}
	
	void Context::throwBadTypeConversion(Token const& token, TypeID from, TypeID to) const {
		throw BadTypeConversion(token, sym.getObjectType(from), sym.getObjectType(to));
	}
	
}

