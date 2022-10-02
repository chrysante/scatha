#include "Sema/SemanticIssue.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Scope.h"

namespace scatha::sema {

	static std::string fullName(Scope const* sc) {
		std::string result(sc->name());
		while (true) {
			sc = sc->parent();
			if (sc == nullptr) { return result; }
			result = std::string(sc->name()) + "." + result;
		}
	};
	
	SemanticIssue::SemanticIssue(Token const& token, std::string_view brief, std::string_view message):
		ProgramIssue(token, brief, message)
	{}
	
	BadTypeConversion::BadTypeConversion(Token const& token, ObjectType const& from, ObjectType const& to):
		TypeIssue(token, utl::strcat("Cannot convert from ", from.name(), " to ", to.name()),
				  "Note: For now we don't allow any implicit conversions")
	{
		
	}
	
	BadFunctionCall::BadFunctionCall(Token const& token, Reason):
		SemanticIssue(token, utl::strcat("No matching function to call for \"", token.id, "\""))
	{
		
	}
	
	UseOfUndeclaredIdentifier::UseOfUndeclaredIdentifier(Token const& token):
		SymbolError(token, utl::strcat("Use of undeclared Identifier \"", token.id, "\""))
	{}
	
	InvalidSymbolReference::InvalidSymbolReference(Token const& token, SymbolCategory actually):
		SymbolError(token, utl::strcat("Identifier \"", token.id, "\" is a ", actually))
	{}
	
	InvalidSymbolReference::InvalidSymbolReference(Token const& token, ast::ExpressionKind actually):
		SymbolError(token, utl::strcat("Identifier \"", token.id, "\" is a ", actually))
	{}
	
	InvalidStatement::InvalidStatement(Token const& token, std::string_view message):
		SemanticIssue(token, message)
	{}
	
	InvalidDeclaration::InvalidDeclaration(Token const& token, Scope const& scope, std::string_view element):
		InvalidStatement(token, utl::strcat("Invalid declaration of ", element, " at ", scope.kind(), " scope"))
	{}
	
	InvalidFunctionDeclaration::InvalidFunctionDeclaration(Token const& token, Scope const& scope):
		InvalidDeclaration(token, scope, "function")
	{}
	
	InvalidOverload::InvalidOverload(Token const& token, Scope const& scope):
		InvalidFunctionDeclaration(token, scope)
	{}
	
	InvalidStructDeclaration::InvalidStructDeclaration(Token const& token, Scope const& scope):
		InvalidDeclaration(token, scope, " struct ")
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, Scope const& scope):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" already declared in scope ", fullName(&scope)))
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, ObjectType const& oldType):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" has previously been declared to be of type \"",  oldType.name(), "\""))
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, Scope const& scope,
											   SymbolCategory existing):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" already declared in scope ", fullName(&scope),
											" as ", toString(existing)))
	{}
	
}
