#ifndef SCATHA_SEMA_SYMBOLISSUE_H_
#define SCATHA_SEMA_SYMBOLISSUE_H_

#include <string_view>

#include "Common/Token.h"
#include "Sema/Exp/SymbolID.h"
#include "Sema/Exp/ScopeKind.h"

namespace scatha::sema::exp {
	
	class SymbolIssue {
	public:
		virtual ~SymbolIssue() = default;
		
		explicit SymbolIssue(Token t): _token(std::move(t)) {}
		Token const& token() const { return _token; }
		
		void setToken(Token);
		
	private:
		Token _token;
	};
	
	class DefinitionIssue: public SymbolIssue {
	public:
		using SymbolIssue::SymbolIssue;
	};
	
	class InvalidScopeIssue: public DefinitionIssue {
	public:
		InvalidScopeIssue(std::string_view symbolName, ScopeKind kind);
		
		ScopeKind kind() const { return _kind; }
		
	private:
		ScopeKind _kind;
	};
	
	class SymbolCollisionIssue: public DefinitionIssue {
	public:
		explicit SymbolCollisionIssue(std::string_view symbolName, SymbolID existing);
		
		SymbolID existing() const { return _existing; }
		
	private:
		SymbolID _existing;
	};
	
	class OverloadIssue: public SymbolCollisionIssue {
	public:
		enum Reason { CantOverloadOnReturnType, Redefinition };
		
	public:
		explicit OverloadIssue(std::string_view symbolName, SymbolID existing, Reason);
		
		Reason reason() const { return _reason; }
		
	private:
		Reason _reason;
	};
	
}

#endif // SCATHA_SEMA_SYMBOLISSUE_H_

