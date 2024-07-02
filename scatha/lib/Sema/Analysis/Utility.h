#ifndef SCATHA_SEMA_ANALYSIS_UTILITY_H_
#define SCATHA_SEMA_ANALYSIS_UTILITY_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

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
QualType getQualType(Type const* type, Mutability mut = Mutability::Mutable,
                     PointerBindMode bindMode = PointerBindMode::Static);

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
ArrayType const* dynArrayTypeCast(Type const* type);

/// \Returns `true` if \p type is a dynamic array type
bool isDynArray(Type const& type);

/// \Returns `true` if the type \p type is an _aggregate_ type.
///
/// An _aggregate_ type is a struct type which has no user defined lifetime
/// functions and no data members which stronger access control than \p type
/// itself
SCATHA_API bool isAggregate(Type const* type);

/// \Returns `true` if \p F is a constructor or destructor
bool isNewMoveDelete(sema::Function const& F);

///
bool isDerivedFrom(RecordType const* derived, RecordType const* base);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_UTILITY_H_
