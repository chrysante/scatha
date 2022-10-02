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
#include "Sema/Prepass.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {

	using namespace ast;
	
	namespace {
		struct Context {
			void analyze(AbstractSyntaxTree&);
			
			void verifyConversion(ast::Expression const& from, TypeID to) const;
			TypeID verifyBinaryOperation(ast::BinaryExpression const&) const;
			void verifyFunctionCallExpression(ast::FunctionCall const&,
											  FunctionSignature const& fnType,
											  std::span<ast::UniquePtr<ast::Expression> const> arguments) const;
			[[noreturn]] void throwBadTypeConversion(Token const&, TypeID from, TypeID to) const;
			
			decltype(auto) withCurrentFunction(FunctionDefinition& fn, std::invocable auto&& f) const {
				auto _this = const_cast<Context*>(this);
				_this->currentFunction = &fn;
				utl::scope_guard pop = [&]{ _this->currentFunction = nullptr; };
				return f();
			}
			
			SymbolTable& sym;
			ast::FunctionDefinition* currentFunction = nullptr;
		};
	} // namespace
	
	SymbolTable analyze(AbstractSyntaxTree* root) {
		SymbolTable sym = prepass(*root);
		Context ctx{ sym };
		ctx.analyze(*root);
		return sym;
	}
	
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
				sym.withScopePushed(block.scopeSymbolID, [&]{
					for (auto& statement: block.statements) {
						analyze(*statement);
					}
				});
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
				
				withCurrentFunction(fn, [&]{
					sym.withScopePushed(fn.symbolID, [&]{
						for (auto& param: fn.parameters) {
							analyze(*param);
						}
					});
					// The body will push the scope itself again
					analyze(*fn.body);
				});
			},
			[&](StructDefinition& s) {
				if (auto const sk = sym.currentScope().kind();
					sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object)
				{
					throw InvalidStructDeclaration(s.token(), sym.currentScope());
				}
				analyze(*s.body);
			},
			[&](VariableDeclaration& var) {
				if (var.symbolID) {
					// We already handled this variable in prepass
					var.typeID = sym.getVariable(var.symbolID).typeID();
					return;
				}
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
				// Declare the variable.
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
				if (sym.is(symbolID, SymbolCategory::Variable)) {
					auto const& var = sym.getVariable(symbolID);
					i.typeID = var.typeID();
				}
				else if (sym.is(symbolID, SymbolCategory::OverloadSet)) {
					i.typeID = TypeID::Invalid;
				}
				else {
					/// TODO: Throw something better here
					throw SemanticIssue(i.token(), "Invalid use of identifier");
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
				auto const& objType = sym.getObjectType(ma.object->typeID);
				auto const memberID = objType.findID(ma.memberName());
				if (!memberID) {
					throw UseOfUndeclaredIdentifier(ma.member());
				}
				if (!sym.is(memberID, SymbolCategory::Variable)) {
					// We don't allow non-variable member accesses yet
					throw;
				}
				auto const& memberVar = sym.getVariable(memberID);
				ma.typeID = memberVar.typeID();
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

