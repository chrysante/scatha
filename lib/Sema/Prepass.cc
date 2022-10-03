#define UTL_DEFER_MACROS

#include "Sema/Prepass.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "AST/Visit.h"
#include "Sema/SemanticIssue.h"
#include "Sema/ExpressionAnalysis.h"

namespace scatha::sema {
	
	/**
	 * In prepass we declare all type including nested types in a first pass and try to figure out their sizes and alignments.
	 * All functions and types of which we can't determine the size because it has members of incomplete or undeclared type will be pushed into a list.
	 * Then that list is repeatedly traversed and successfully registered declarations will be removed from the list until it is empty or its size does not change anymore.
	 * Then a last pass over the list is run and we collect the appropriate errors.
	 */
	
	namespace {
		
		struct StatementContext {
			ast::Statement* statement;
			Scope* enclosingScope;
		};
		
		struct PrepassContext {
			explicit PrepassContext(SymbolTable& sym, issue::IssueHandler& iss):
				sym(sym),
			iss(iss)
			{}
			
			bool dispatch(ast::AbstractSyntaxTree&);
			
			bool prepass(ast::TranslationUnit&);
			bool prepass(ast::FunctionDefinition&);
			bool prepass(ast::StructDefinition&);
			bool prepass(ast::VariableDeclaration&);
			/// Disable default case to avoid infinite recursion
			bool prepass(ast::AbstractSyntaxTree&) { return true; }
			bool prepass(ast::Expression&) { SC_DEBUGFAIL(); }
			
			ExpressionAnalysisResult dispatchExpression(ast::Expression&);
			
			void markUnhandled(ast::Statement*);
			
			SymbolTable& sym;
			issue::IssueHandler& iss;
			utl::vector<StatementContext> unhandledStatements;
			ast::FunctionDefinition* currentFunction;
			bool firstPass = true;
			bool lastPass = false;
		};
		
	} // namespace
		
	SymbolTable prepass(ast::AbstractSyntaxTree& root, issue::IssueHandler& iss) {
		SymbolTable sym;
		PrepassContext ctx{ sym, iss };
		ctx.dispatch(root);
		if (iss.fatal()) {
			return sym;
		}
		ctx.firstPass = false;
		while (!ctx.unhandledStatements.empty()) {
			auto const beginSize = ctx.unhandledStatements.size();
			for (size_t i = 0; i < ctx.unhandledStatements.size(); ) {
				auto const x = ctx.unhandledStatements[i];
				bool const success = sym.withScopeCurrent(x.enclosingScope, [&]{
					return ctx.dispatch(*x.statement);
				});
				if (success || ctx.lastPass) {
					ctx.unhandledStatements.erase(ctx.unhandledStatements.begin() + i);
					continue;
				}
				else if (iss.fatal()) {
					return sym;
				}
				++i;
			}
			if (ctx.unhandledStatements.size() == beginSize) { // did not decrease in size
				ctx.lastPass = true;
			}
		}
		return sym;
	}
	
	bool PrepassContext::dispatch(ast::AbstractSyntaxTree& node) {
		return visit(node, [this](auto& node) { return this->prepass(node); });
	}
	
	bool PrepassContext::prepass(ast::TranslationUnit& tu) {
		for (auto& decl: tu.declarations) {
			dispatch(*decl);
			if (iss.fatal()) {
				return false;
			}
		}
		return true;
	}
	
	bool PrepassContext::prepass(ast::FunctionDefinition& fn) {
		if (firstPass) {
			markUnhandled(&fn);
			return false;
		}
		if (auto const sk = sym.currentScope().kind();
			sk != ScopeKind::Global &&
			sk != ScopeKind::Namespace &&
			sk != ScopeKind::Object)
		{
			/// Function defintion is only allowed in the global scope, at namespace scope and structure scope
			iss.push(InvalidDeclaration(&fn, InvalidDeclaration::Reason::InvalidInCurrentScope,
										sym.currentScope(), SymbolCategory::Function));
			return false;
		}
		
		/// Analyze the return argument expressions
		utl::small_vector<TypeID> argTypes;
		for (auto& param: fn.parameters) {
			auto const typeExprRes = dispatchExpression(*param->typeExpr);
			if (!typeExprRes) {
				if (!lastPass) { return false; }
				argTypes.push_back(TypeID::Invalid);
				continue;
			}
			if (typeExprRes.category() != ast::EntityCategory::Type) {
				if (lastPass) {
					iss.push(BadSymbolReference(*param->typeExpr, param->typeExpr->category, ast::EntityCategory::Type));
				}
				argTypes.push_back(TypeID::Invalid);
				continue;
			}
			auto const& type = sym.getObjectType(typeExprRes.typeID());
			argTypes.push_back(type.symbolID());
		}
		/// Analyze the return type expression
		auto const returnTypeExprRes = dispatchExpression(*fn.returnTypeExpr);
		if (iss.fatal()) { return false; }
		if (!returnTypeExprRes && !lastPass) {
			return false;
		}
		TypeID returnTypeID;
		if (returnTypeExprRes) {
			if (returnTypeExprRes.category() != ast::EntityCategory::Type) {
				if (lastPass) {
					iss.push(BadSymbolReference(*fn.returnTypeExpr, fn.returnTypeExpr->category, ast::EntityCategory::Type));
				}
			}
			returnTypeID = returnTypeExprRes.typeID();
		}
		/// Declare the function. If we get here before the last bailout pass, check that our types are valid
		if (!lastPass) {
			if (!returnTypeID) { return false; }
			for (auto id: argTypes) {
				if(!id) { return false; }
			}
		}
		/// Might be TypeID::Invalid if we are in the last pass but we still declare the function and go on
		fn.returnTypeID = returnTypeID;
		Expected func = sym.addFunction(fn.token(), FunctionSignature(argTypes, returnTypeID));
		if (!func.hasValue()) {
			if (lastPass) {
				func.error().setStatement(fn);
				iss.push(func.error());
			}
			return false;
		}
		fn.symbolID = func->symbolID();
		fn.functionTypeID = func->typeID();
		fn.body->scopeKind = ScopeKind::Function;
		fn.body->scopeSymbolID = fn.symbolID;
		return true;
	}
	
	bool PrepassContext::prepass(ast::StructDefinition& s) {
		if (auto const sk = sym.currentScope().kind();
			sk != ScopeKind::Global &&
			sk != ScopeKind::Namespace &&
			sk != ScopeKind::Object)
		{
			/// Struct defintion is only allowed in the global scope, at namespace scope and structure scope
			iss.push(InvalidDeclaration(&s, InvalidDeclaration::Reason::InvalidInCurrentScope,
										sym.currentScope(), SymbolCategory::ObjectType));
			return false;
		}
		auto obj = [&]() -> Expected<ObjectType&, SemanticIssue> {
			if (firstPass) {
				return sym.addObjectType(s.token());
			}
			else {
				return sym.getObjectType(s.symbolID);
			}
		}();
		if (!obj) {
			obj.error().setStatement(s);
			iss.push(obj.error());
			return false;
		}
		
		s.symbolID = obj->symbolID();
		
		size_t objectSize = 0;
		size_t objectAlign = 1;
		bool successfullyGatheredVarDecls = true;
		sym.pushScope(obj->symbolID());
		utl::armed_scope_guard popScope = [&]{ sym.popScope(); };
		for (auto& statement: s.body->statements) {
			if (firstPass || statement->nodeType() == ast::NodeType::VariableDeclaration) {
				dispatch(*statement);
				if (iss.fatal()) { return false; }
			}
			if (statement->nodeType() != ast::NodeType::VariableDeclaration) {
				continue;
			}
			auto const& varDecl = static_cast<ast::VariableDeclaration&>(*statement);
			auto const* typenameIdentifier = downCast<ast::Identifier>(varDecl.typeExpr.get());
			SC_ASSERT(typenameIdentifier, "must be identifier for now");
			auto const* type = sym.lookupObjectType(typenameIdentifier->value());
			if (!type || !type->isComplete()) {
				successfullyGatheredVarDecls = false;
				if (lastPass) {
					iss.push(UseOfUndeclaredIdentifier(*typenameIdentifier, sym.currentScope()));
				}
				if (firstPass) {
					continue;
				}
				break;
			}
			objectAlign = std::max(objectAlign, type->align());
			objectSize = utl::round_up_pow_two(objectSize + type->size(), type->align());
		}
		popScope.execute();
		if (!successfullyGatheredVarDecls) {
			if (firstPass) { markUnhandled(&s); }
			return false;
		}
		s.body->scopeKind = ScopeKind::Object;
		s.body->scopeSymbolID = s.symbolID;
		obj->setSize(utl::round_up_pow_two(objectSize, objectAlign));
		obj->setAlign(objectAlign);
		return true;
	}
	
	bool PrepassContext::prepass(ast::VariableDeclaration& decl) {
		SC_ASSERT(sym.currentScope().kind() == ScopeKind::Object,
				  "We only want to prepass struct definitions. What are we doing here?");
		SC_ASSERT(decl.typeExpr, "In structs variables need explicit type specifiers. Make this a program issue.");
		auto const* typenameIdentifier = downCast<ast::Identifier>(decl.typeExpr.get());
		SC_ASSERT(typenameIdentifier, "must be identifier for now");
		auto const* typePtr = sym.lookupObjectType(typenameIdentifier->value());
		TypeID const typeID = typePtr ? typePtr->symbolID() : TypeID::Invalid;
		auto var = [&]() -> Expected<Variable&, SemanticIssue> {
			if (firstPass) {
				return sym.addVariable(decl.token(), typeID, bool{});
			}
			else {
				return sym.getVariable(decl.symbolID);
			}
		}();
		if (!var) {
			var.error().setStatement(decl);
			iss.push(var.error());
			return false;
		}
		decl.symbolID = var->symbolID();
		var->setTypeID(typeID);
		return typeID != TypeID::Invalid;
	}

	ExpressionAnalysisResult PrepassContext::dispatchExpression(ast::Expression& expr) {
		return analyzeExpression(expr, sym, lastPass ? &iss : nullptr);
	}
	
	void PrepassContext::markUnhandled(ast::Statement* statement) {
		unhandledStatements.push_back({ statement, &sym.currentScope() });
	}
	
}
