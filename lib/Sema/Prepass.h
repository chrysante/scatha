#ifndef SCATHA_SEMA_PREPASS_H_
#define SCATHA_SEMA_PREPASS_H_

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {
	
	SCATHA(API) SymbolTable prepass(ast::AbstractSyntaxTree&);
	
}

#endif // SCATHA_SEMA_PREPASS_H_
