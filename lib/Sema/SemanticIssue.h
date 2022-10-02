#ifndef SCATHA_SEMA_SEMANTICISSUE_H_
#define SCATHA_SEMA_SEMANTICISSUE_H_

#include <stdexcept>
#include <string>

#include "AST/Common.h"
#include "AST/Expression.h"
#include "Common/ProgramIssue.h"
#include "Common/Token.h"
#include "Sema/ObjectType.h"

namespace scatha::sema {

	class Scope;
	
	/// MARK: SemanticIssue
	/// Base class of all semantic errors
	class SCATHA(API) SemanticIssue: public ProgramIssue {
	public:
		SemanticIssue(Token const& token, std::string_view brief, std::string_view message = {});
	};
	
	/// MARK: TypeIssue
	/// Base class of all type related errors
	class SCATHA(API) TypeIssue: public SemanticIssue {
	public:
		using SemanticIssue::SemanticIssue;
	};
	
	class SCATHA(API) BadTypeConversion: public TypeIssue {
	public:
		explicit BadTypeConversion(Token const& token, ObjectType const& from, ObjectType const& to);
	};
	
	class SCATHA(API) BadFunctionCall: public SemanticIssue {
	public:
		enum Reason {
			WrongArgumentCount, NoMatchingFunction
		};
		explicit BadFunctionCall(Token const& token, Reason);
	};
	
	/// MARK: SymbolError
	class SCATHA(API) SymbolError: public SemanticIssue {
	protected:
		using SemanticIssue::SemanticIssue;
	};
	
	class SCATHA(API) UseOfUndeclaredIdentifier: public SymbolError {
	public:
		UseOfUndeclaredIdentifier(Token const& token);
		UseOfUndeclaredIdentifier(ast::Expression const& expr, Scope const& scope);
		
	private:
		static std::string makeMessage(std::string_view, Scope const*);
	};
	
	class SCATHA(API) InvalidSymbolReference: public SymbolError {
	public:
		InvalidSymbolReference(Token const& token, SymbolCategory actually);
		InvalidSymbolReference(Token const& token, ast::ExpressionKind actually);
	};

	/// MARK: StatementError
	class SCATHA(API) InvalidStatement: public SemanticIssue {
	public:
		using SemanticIssue::SemanticIssue;
		InvalidStatement(Token const&, std::string_view message);
	};

	class SCATHA(API) InvalidDeclaration: public InvalidStatement {
	protected:
		InvalidDeclaration(Token const& token, Scope const& scope, std::string_view element);
	};
	
	class SCATHA(API) InvalidFunctionDeclaration: public InvalidDeclaration {
	public:
		InvalidFunctionDeclaration(Token const& token, Scope const& scope);
	};
	
	class SCATHA(API) InvalidOverload: public InvalidFunctionDeclaration {
	public:
		InvalidOverload(Token const& token, Scope const& scope);
	};
	
	class SCATHA(API) InvalidStructDeclaration: public InvalidDeclaration {
	public:
		InvalidStructDeclaration(Token const& token, Scope const& scope);
	};
	
	class SCATHA(API) InvalidRedeclaration: public InvalidStatement {
	public:
		InvalidRedeclaration(Token const& token, Scope const& scope);
		InvalidRedeclaration(Token const& token, ObjectType const& oldType);
		InvalidRedeclaration(Token const& token, Scope const& scope,
							 SymbolCategory existing);
	};
	
}

#endif // SCATHA_SEMA_SEMANTICISSUE_H_


