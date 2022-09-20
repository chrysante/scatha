#include "SemanticError.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Scope.h"

namespace scatha::sema {

	static std::string fullName(Scope const* sc) {
		std::string result(sc->name());
		while (true) {
			sc = sc->parentScope();
			if (sc == nullptr) { return result; }
			result = std::string(sc->name()) + "." + result;
		}
	};
	
	SemanticError::SemanticError(Token const& token, std::string_view brief, std::string_view message):
		std::runtime_error(makeString(brief, token, message))
	{}
	
	std::string SemanticError::makeString(std::string_view brief, Token const& token, std::string_view message) {
		std::stringstream sstr;
		sstr << brief << " at Line: " << token.sourceLocation.line << " Col: " << token.sourceLocation.column;
		if (!message.empty()) {
			sstr << ": \n\t" << message;
		}
		return sstr.str();
	}
	
	BadTypeConversion::BadTypeConversion(Token const& token, TypeEx const& from, TypeEx const& to):
		TypeError(token, utl::strcat("Cannot convert from ", from.name(), " to ", to.name()),
				  "Note: For now we don't allow any implicit conversions")
	{
		
	}
	
	BadFunctionCall::BadFunctionCall(Token const& token, Reason):
		SemanticError(token, utl::strcat("No matching function to call for \"", token.id, "\""))
	{
		
	}
	
	UseOfUndeclaredIdentifier::UseOfUndeclaredIdentifier(Token const& token):
		SymbolError(token, utl::strcat("Use of undeclared Identifier \"", token.id, "\""))
	{}
	
	InvalidSymbolReference::InvalidSymbolReference(Token const& token, SymbolCategory actually):
		SymbolError(token, utl::strcat("Identifier \"", token.id, "\" is a ", toString(actually)))
	{}
	
	InvalidStatement::InvalidStatement(Token const& token, std::string_view message):
		SemanticError(token, message)
	{}
	
	InvalidDeclaration::InvalidDeclaration(Token const& token, Scope const* scope, std::string_view element):
		InvalidStatement(token, utl::strcat("Invalid declaration of ", element, " at ", toString(scope->kind())))
	{}
	
	InvalidFunctionDeclaration::InvalidFunctionDeclaration(Token const& token, Scope const* scope):
		InvalidDeclaration(token, scope, " function ")
	{}
	
	InvalidStructDeclaration::InvalidStructDeclaration(Token const& token, Scope const* scope):
		InvalidDeclaration(token, scope, " function ")
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, Scope const* scope):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" already declared in scope ", fullName(scope)))
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, TypeEx const& oldType):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" has previously been declared to be of type \"", oldType.isFunctionType() ? "<function-type>" : oldType.name(), "\""))
	{}
	
	InvalidRedeclaration::InvalidRedeclaration(Token const& token, Scope const* scope,
											   SymbolCategory existing):
		InvalidStatement(token, utl::strcat("Identifier \"", token.id, "\" already declared in scope ", fullName(scope),
											" as ", toString(existing)))
	{}
	
}
