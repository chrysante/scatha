#ifndef SCATHA_IC_TACGENERATOR_H_
#define SCATHA_IC_TACGENERATOR_H_

#include "AST/Base.h"
#include "IC/ThreeAddressCode.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	SCATHA(API) ThreeAddressCode generateTac(ast::AbstractSyntaxTree const&, sema::SymbolTable const&);
	
}

#endif // SCATHA_IC_TACGENERATOR_H_

