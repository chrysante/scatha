#ifndef SCATHA_SEMA_ANALYSIS_UTILITY_H_
#define SCATHA_SEMA_ANALYSIS_UTILITY_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// # Destructors

/// If the expression \p expr is of non-trivial lifetime type, this function
/// pops the top element off the destructor stack \p dtors
void popTopLevelDtor(ast::Expression* expr, CleanupStack& dtors);

/// # Special lifetime functions

/// Declares the appropriate special lifetime functions of \p type
/// This is used by Instantiation to analyze structs and by symbol table to
/// generate array types.
void declareSpecialLifetimeFunctions(ObjectType& type, SymbolTable& sym);

/// # Other utils

/// \Returns the function in the list \p functions that exactly matches the
/// parameter types \p paramTypes
Function* findBySignature(std::span<Function* const> functions,
                          std::span<Type const* const> paramTypes);

/// \overload
Function const* findBySignature(std::span<Function const* const> set,
                                std::span<Type const* const> paramTypes);

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

/// \Returns \p type downcast to `ArrayType` if it is a dynamic array type, null
/// otherwise
ArrayType const* dynArrayTypeCast(sema::Type const* type);

/// \Returns `true` if \p type is a dynamic array type
bool isDynArray(sema::Type const& type);

/// Inserts a `{Triv,Nontriv}CopyConstructExpr` above \p expr
ast::Expression* insertConstruction(ast::Expression* expr,
                                    CleanupStack& dtors,
                                    AnalysisContext& ctx);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_UTILITY_H_
