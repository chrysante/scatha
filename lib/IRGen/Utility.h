#ifndef SCATHA_IRGEN_UTILITY_H_
#define SCATHA_IRGEN_UTILITY_H_

#include <optional>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/Value.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// \Returns the referred to type if \p type is a pointer or reference type.
/// Otherwise returns `nullptr`
sema::ObjectType const* getPtrOrRefBase(sema::Type const* type);

/// \Returns `true` if \p type is a pointer or a reference to an array
bool isPtrOrRefToArray(sema::Type const* type);

/// \Returns `true` if \p type is a pointer or a reference to an array with
/// dynamic size
bool isFatPointer(sema::Type const* type);

/// \overload for expressions
bool isFatPointer(ast::Expression const* expr);

/// \Returns the size if \p type is a statically sized array or a pointer or
/// reference thereto
std::optional<size_t> getStaticArraySize(sema::Type const* type);

/// Creates an anonymous struct type with members `ptr` and `i64`
ir::StructType const* makeArrayViewType(ir::Context& ctx);

/// \Returns `Register` if either \p a or \p b is `Register`, otherwise returns
/// `Memory`
ValueLocation commonLocation(ValueLocation a, ValueLocation b);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_UTILITY_H_
