#ifndef SCATHA_IRGEN_GLOBALDECLS_H_
#define SCATHA_IRGEN_GLOBALDECLS_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"
#include "IRGen/Metadata.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class GlobalMap;
class TypeMap;

/// Generates the lowering metadata for \p semaType
StructMetadata makeStructMetadata(TypeMap& typeMap,
                                  sema::StructType const* semaType);

/// Translates \p semaType to an IR structure type
ir::StructType* generateType(sema::RecordType const* semaType, ir::Context& ctx,
                             ir::Module& mod, TypeMap& typeMap,
                             sema::NameMangler const& nameMangler);

///
CallingConvention computeCallingConvention(sema::Function const& function);

/// Translates the function declaration \p semaFn to an IR function.
/// \Note This does not generate code
ir::Callable* declareFunction(sema::Function const& semaFn, ir::Context& ctx,
                              ir::Module& mod, TypeMap const& typeMap,
                              GlobalMap& globalMap,
                              sema::NameMangler const& nameMangler);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_GLOBALDECLS_H_
