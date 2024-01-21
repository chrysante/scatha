#ifndef SCATHA_IRGEN_UTILITY_H_
#define SCATHA_IRGEN_UTILITY_H_

#include <array>
#include <optional>

#include "AST/Fwd.h"
#include "IR/Fwd.h"
#include "IRGen/Value.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// The size limit objects that we want to keep in registers
static size_t const PreferredMaxRegisterValueSize = 16;

/// \Returns the referred to type if \p type is a pointer or reference type.
/// Otherwise returns `nullptr`
sema::ObjectType const* getPtrOrRefBase(sema::Type const* type);

/// \Returns `true` if \p type is an array with
/// dynamic size or a pointer or a reference thereto
bool isFatPointer(sema::Type const* type);

/// \overload for expressions
bool isFatPointer(ast::Expression const* expr);

/// \Returns the size if \p type is a statically sized array or a pointer or
/// reference thereto
std::optional<size_t> getStaticArraySize(sema::Type const* type);

/// Creates an anonymous struct type with members `ptr` and `i64`
ir::StructType const* makeArrayPtrType(ir::Context& ctx);

/// \Returns \p a if `a == b` or \p fallback otherwise
inline ValueLocation commonLocation(
    ValueLocation a,
    ValueLocation b,
    ValueLocation fallback = ValueLocation::Register) {
    return a == b ? a : fallback;
}

/// \Returns \p a if `a == b` or \p fallback otherwise
inline ValueRepresentation commonRepresentation(
    ValueRepresentation a,
    ValueRepresentation b,
    ValueRepresentation fallback = ValueRepresentation::Packed) {
    return a == b ? a : fallback;
}

/// Convenience wrapper to make `std::array<size_t, N>` in a less verbose way
template <size_t N>
struct IndexArray: std::array<std::size_t, N> {};

template <typename... T>
IndexArray(T...) -> IndexArray<sizeof...(T)>;

/// Convenience wrapper to make `std::array<ir::Value*, N>` in a less verbose
/// way
template <size_t N>
struct ValueArray: std::array<ir::Value*, N> {};

template <typename... T>
ValueArray(T...) -> ValueArray<sizeof...(T)>;

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_UTILITY_H_
