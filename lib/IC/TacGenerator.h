#ifndef SCATHA_IC_TACGENERATOR_H_
#define SCATHA_IC_TACGENERATOR_H_

#include "AST/AST.h"
#include "IC/ThreeAddressCode.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {

SCATHA(API) ThreeAddressCode generateTac(ast::AbstractSyntaxTree const&, sema::SymbolTable const&);

} // namespace scatha::ic

#endif // SCATHA_IC_TACGENERATOR_H_
