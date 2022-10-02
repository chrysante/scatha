#define UTL_DEFER_MACROS

#include "Sema/Prepass.h"

#include <vector>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "AST/Visit.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {
	
	using namespace ast;
	
	namespace {
		
		struct StatementContext {
			Statement* statement;
			Scope* enclosingScope;
		};
		
		struct PrepassContext {
			explicit PrepassContext(SymbolTable& sym): sym(sym) {}
			
			bool prepass(ast::AbstractSyntaxTree&);
			
			void markUnhandled(ast::Statement*);
			
			SymbolTable& sym;
			std::vector<StatementContext> unhandledStatements;
			ast::FunctionDefinition* currentFunction;
			bool firstPass = true;
			bool lastPass = false;
		};
		
	}
		
	SymbolTable prepass(ast::AbstractSyntaxTree& root) {
		SymbolTable sym;
		PrepassContext ctx{ sym };
		ctx.prepass(root);
		ctx.firstPass = false;
		while (!ctx.unhandledStatements.empty()) {
			auto const beginSize = ctx.unhandledStatements.size();
			for (size_t i = 0; i < ctx.unhandledStatements.size(); ) {
				auto const x = ctx.unhandledStatements[i];
				bool const success = sym.withScopeCurrent(x.enclosingScope, [&]{
					return ctx.prepass(*x.statement);
				});
				if (success) {
					ctx.unhandledStatements.erase(ctx.unhandledStatements.begin() + i);
					continue;
				}
				++i;
			}
			if (ctx.unhandledStatements.size() == beginSize) { // did not decrease in size
				ctx.lastPass = true;
			}
		}
		return sym;
	}
	
	bool PrepassContext::prepass(AbstractSyntaxTree& node) {
		auto const vis = utl::visitor{
			[&](TranslationUnit& tu) {
				for (auto& decl: tu.declarations) {
					prepass(*decl);
				}
				return true;
			},
			[&](Block& block) {
				SC_DEBUGFAIL();
				if (block.scopeKind != ScopeKind::Global &&
					block.scopeKind != ScopeKind::Namespace &&
					block.scopeKind != ScopeKind::Object)
				{
					// Don't care about these blocks in prepass
					return true;
				}
				sym.pushScope(block.scopeSymbolID);
				utl_defer { sym.popScope(); };
				for (auto& statement: block.statements) {
					prepass(*statement);
				}
				return true;
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
				auto const* typenameIdentifier = downCast<Identifier>(fn.returnTypeExpr.get());
				SC_ASSERT(typenameIdentifier, "must be identifier for now");
				auto const* returnTypePtr = sym.lookupObjectType(typenameIdentifier->value());
				if (!returnTypePtr) {
					if (firstPass) {
						markUnhandled(&fn);
					}
					if (lastPass) {
						throw UseOfUndeclaredIdentifier(typenameIdentifier->token());
					}
					return false;
				}
				auto const& returnType = *returnTypePtr;
				fn.returnTypeID = returnType.symbolID();
				utl::small_vector<TypeID> argTypes;
				for (auto& param: fn.parameters) {
					auto const* typenameIdentifier = downCast<Identifier>(param->typeExpr.get());
					SC_ASSERT(typenameIdentifier, "must be identifier for now");
					auto const* typePtr = sym.lookupObjectType(typenameIdentifier->value());
					if (!typePtr) {
						if (firstPass) {
							markUnhandled(&fn);
						}
						if (lastPass) {
							throw UseOfUndeclaredIdentifier(typenameIdentifier->token());
						}
						return false;
					}
					argTypes.push_back(typePtr->symbolID());
				}
				Expected const func = sym.addFunction(fn.token(), FunctionSignature(argTypes, returnType.symbolID()));
				if (!func.hasValue()) {
					throw InvalidRedeclaration(fn.token(), sym.currentScope());
				}
				fn.symbolID = func->symbolID();
				fn.functionTypeID = func->typeID();
				fn.body->scopeKind = ScopeKind::Function;
				fn.body->scopeSymbolID = fn.symbolID;
				return true;
			},
			[&](StructDefinition& s) {
				if (auto const sk = sym.currentScope().kind();
					sk != ScopeKind::Global &&
					sk != ScopeKind::Namespace &&
					sk != ScopeKind::Object)
				{
					/*
					 * Struct defintion is only allowed in the global scope, at namespace scope and structure scope
					 */
					throw InvalidStructDeclaration(s.token(), sym.currentScope());
				}
				auto& obj = [&]() -> auto& {
					if (firstPass) {
						auto obj = sym.addObjectType(s.token());
						if (!obj) {
							throw InvalidRedeclaration(s.token(), sym.currentScope());
						}
						s.symbolID = obj->symbolID();
						return obj.value();
					}
					else {
						return sym.getObjectType(s.symbolID);
					}
				}();
				size_t objectSize = 0;
				size_t objectAlign = 1;
				bool success = true;
				sym.withScopePushed(obj.symbolID(), [&]{
					for (auto& statement: s.body->statements) {
						if (!firstPass && statement->nodeType() == ast::NodeType::StructDefinition) { continue; }
						if (!firstPass && statement->nodeType() == ast::NodeType::FunctionDefinition) { continue; }
						if (!isDeclaration(statement->nodeType())) {
							throw InvalidStatement(statement->token(), "Expected declaration");
						}
						prepass(*statement);
						if (statement->nodeType() == ast::NodeType::VariableDeclaration) {
							auto const& varDecl = static_cast<VariableDeclaration&>(*statement);
							auto const* typenameIdentifier = downCast<Identifier>(varDecl.typeExpr.get());
							SC_ASSERT(typenameIdentifier, "must be identifier for now");
							auto const* type = sym.lookupObjectType(typenameIdentifier->value());
							if (!type || !type->isComplete()) {
								success = false;
								if (lastPass) {
									throw UseOfUndeclaredIdentifier(typenameIdentifier->token());
								}
								if (firstPass) {
									continue;
								}
								else {
									break;
								}
							}
							objectAlign = std::max(objectAlign, type->align());
							objectSize = utl::round_up_pow_two(objectSize + type->size(), type->align());
						}
					}
				});
				if (!success) {
					if (firstPass) {
						markUnhandled(&s);						
					}
					return false;
				}
				s.body->scopeKind = ScopeKind::Object;
				s.body->scopeSymbolID = s.symbolID;
				obj.setSize(utl::round_up_pow_two(objectSize, objectAlign));
				obj.setAlign(objectAlign);
				return true;
			},
			[&](VariableDeclaration& decl) {
				SC_ASSERT(sym.currentScope().kind() == ScopeKind::Object,
						  "We only want to prepass struct definitions. What are we doing here?");
				SC_ASSERT(decl.typeExpr, "In structs variables need explicit type specifiers. Make this a program issue.");
				auto const* typenameIdentifier = downCast<Identifier>(decl.typeExpr.get());
				SC_ASSERT(typenameIdentifier, "must be identifier for now");
				auto const* typePtr = sym.lookupObjectType(typenameIdentifier->value());
				TypeID const typeID = typePtr ? typePtr->symbolID() : TypeID::Invalid;
				auto& var = [&]() -> auto& {
					if (firstPass) {
						auto var = sym.addVariable(decl.token(), typeID, bool{});
						if (!var) {
							throw InvalidRedeclaration(decl.token(), sym.currentScope());
						}
						decl.symbolID = var->symbolID();
						return var.value();
					}
					else {
						return sym.getVariable(decl.symbolID);
					}
				}();
				var.setTypeID(typeID);
				return typeID != TypeID::Invalid;
			},
			[&](auto&&) { return true; }
		};
		return visit(node, vis);
	}
	
	void PrepassContext::markUnhandled(ast::Statement* statement) {
		unhandledStatements.push_back({ statement, &sym.currentScope() });
	}
	
}
