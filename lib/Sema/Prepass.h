#ifndef SCATHA_SEMA_PREPASS_H_
#define SCATHA_SEMA_PREPASS_H_

#include <optional>

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/ObjectType.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

SCATHA(API) SymbolTable prepass(ast::AbstractSyntaxTree&, issue::IssueHandler&);

} // namespace scatha::sema

#endif // SCATHA_SEMA_PREPASS_H_
