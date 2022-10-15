#ifndef SCATHA_IC_CANONICALIZE_H_
#define SCATHA_IC_CANONICALIZE_H_

#include "AST/AST.h"

namespace scatha::ic {

void SCATHA(API) canonicalize(ast::AbstractSyntaxTree*);

}

#endif // SCATHA_IC_CANONICALIZE_H_
