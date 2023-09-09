#ifndef SCATHA_IRGEN_GLOBALS_H_
#define SCATHA_IRGEN_GLOBALS_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class TypeMap;
struct StructMetaData;
struct FunctionMetaData;

/// Translates \p semaType to an IR structure type
std::pair<UniquePtr<ir::StructType>, StructMetaData> generateType(
    ir::Context& ctx, TypeMap& typeMap, sema::StructType const* semaType);

/// Translates the function declaration \p semaFn to an IR function.
/// \Note This does not generate code
std::pair<UniquePtr<ir::Callable>, FunctionMetaData> declareFunction(
    ir::Context& ctx, TypeMap& typeMap, sema::Function const* semaFn);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_GLOBALS_H_
