#ifndef SCATHA_SEMA_ANALYSIS_UTILITY_H_
#define SCATHA_SEMA_ANALYSIS_UTILITY_H_

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// # Destructors

/// If the expression \p expr is of non-trivial lifetime type, this function
/// pops the top element off the destructor stack \p dtors
void popTopLevelDtor(ast::Expression* expr, DtorStack& dtors);

/// # Special lifetime functions

/// Declares the appropriate special lifetime functions of \p type
/// This is used by Instantiation to analyze structs and by symbol table to
/// generate array types.
void declareSpecialLifetimeFunctions(ObjectType& type, SymbolTable& sym);

/// # Other utils

/// \Returns the type referenced by \p type if \p type is a reference type.
/// Otherwise returns \p type as is
QualType getQualType(Type const* type, Mutability mut = Mutability::Mutable);

/// \Returns `LValue` if \p type is a reference type, otherwise returns `RValue`
ValueCategory refToLValue(Type const* type);

/// \Returns the first statement that is an ancestor of \p node or `nullptr` if
/// nonesuch exists
ast::Statement* parentStatement(ast::ASTNode* node);

/// \returns \p type downcast to `CompoundType`, if \p type is a compound type
/// with non-trivial lifetime. Otherwise returns `nullptr`
CompoundType const* nonTrivialLifetimeType(ObjectType const* type);

/// Computes the access control of \p decl
/// If \p decl has explicitly specified access control that value returned.
/// Otherwise returns `determineAccessControlByContext(scope)`
AccessControl determineAccessControl(Scope const& scope,
                                     ast::Declaration const& decl);

///
AccessControl determineAccessControlByContext(Scope const& scope);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_UTILITY_H_
