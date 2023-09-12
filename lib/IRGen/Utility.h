#ifndef SCATHA_IRGEN_UTILITY_H_
#define SCATHA_IRGEN_UTILITY_H_

#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// \Returns pointee type as array type, if \p type is a pointer to array,
/// `nullptr` otherwise
sema::ArrayType const* ptrToArray(sema::ObjectType const* type);

/// \Returns the base type if \p type is a pointer or reference type, otherwise
/// returns \p type as is
sema::QualType stripRefOrPtr(sema::QualType type);

///
bool isArrayAndDynamic(sema::ObjectType const* type);

///
bool isArrayPtrOrArrayRef(sema::ObjectType const* type);

///
ir::StructType const* makeArrayViewType(ir::Context& ctx);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_UTILITY_H_
