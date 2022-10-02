#ifndef SCATHA_SEMA_PREPASS_H_
#define SCATHA_SEMA_PREPASS_H_

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Sema/SymbolTable.h"
#include "Sema/ObjectType.h"

namespace scatha::sema {
	
	SCATHA(API) SymbolTable prepass(ast::AbstractSyntaxTree&);
	
	ObjectType* lookupType(ast::AbstractSyntaxTree const&, SymbolTable&);
	
	bool tryAnalyzeIdentifier(ast::Identifier&, SymbolTable&,
							  bool allowFailure, bool lookupStrict);
	bool tryAnalyzeMemberAccess(ast::MemberAccess&, SymbolTable&,
								bool allowFailure, bool lookupStrict);
	
}

#endif // SCATHA_SEMA_PREPASS_H_
