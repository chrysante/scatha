#ifndef SCATHA_SEMANTICANALYZER_SEMANTICERROR_H_
#define SCATHA_SEMANTICANALYZER_SEMANTICERROR_H_

#include <stdexcept>
#include <string>

#include "Common/Token.h"
#include "SemanticAnalyzer/SemanticElements.h"

namespace scatha::sem {

	class Scope;
	
	/// MARK: SemanticError
	/// Base class of all semantic errors
	class SemanticError: public std::runtime_error {
	public:
		SemanticError(Token const& token, std::string_view brief, std::string_view message = {});
		
	private:
		static std::string makeString(std::string_view, Token const&, std::string_view);
	};
	
	/// MARK: TypeError
	/// Base class of all type related errors
	class TypeError: public SemanticError {
	public:
		using SemanticError::SemanticError;

	private:
		
	};
	
	class BadTypeConversion: public TypeError {
	public:
		explicit BadTypeConversion(Token const& token, TypeEx const& from, TypeEx const& to);
	};
	
	class BadFunctionCall: public SemanticError {
	public:
		enum Reason {
			WrongArgumentCount
		};
		explicit BadFunctionCall(Token const& token, Reason);
	};
	
	/// MARK: SymbolError
	class SymbolError: public SemanticError {
	protected:
		using SemanticError::SemanticError;
	};
	
	class UseOfUndeclaredIdentifier: public SymbolError {
	public:
		UseOfUndeclaredIdentifier(Token const& token);
	};
	
	class InvalidSymbolReference: public SymbolError {
	public:
		InvalidSymbolReference(Token const& token, NameCategory actually);
	};

	/// MARK: StatementError
	class InvalidStatement: public SemanticError {
	public:
		using SemanticError::SemanticError;
		InvalidStatement(Token const&, std::string_view message);
	};

	class InvalidDeclaration: public InvalidStatement {
	protected:
		InvalidDeclaration(Token const& token, Scope const* scope, std::string_view element);
	};
	
	class InvalidFunctionDeclaration: public InvalidDeclaration {
	public:
		InvalidFunctionDeclaration(Token const& token, Scope const* scope);
	};
	
	class InvalidStructDeclaration: public InvalidDeclaration {
	public:
		InvalidStructDeclaration(Token const& token, Scope const* scope);
	};
	
	class InvalidRedeclaration: public InvalidStatement {
	public:
		InvalidRedeclaration(Token const& token, Scope const* scope);
		InvalidRedeclaration(Token const& token, TypeEx const& oldType);
		InvalidRedeclaration(Token const& token, Scope const* scope,
							 NameCategory existing);
	};
	
}

#endif // SCATHA_SEMANTICANALYZER_SEMANTICERROR_H_


