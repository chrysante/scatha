#ifndef SCATHA_IRGEN_GLOBALDECLS_H_
#define SCATHA_IRGEN_GLOBALDECLS_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"
#include "IRGen/LoweringContext.h"
#include "IRGen/Metadata.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class GlobalMap;
class TypeMap;

/// Generates the lowering metadata for \p semaType
RecordMetadata makeRecordMetadata(sema::RecordType const* semaType,
                                  LoweringContext lctx);

/// Translates \p semaType to an IR structure type
ir::StructType* generateType(sema::RecordType const* semaType,
                             LoweringContext lctx);

///
CallingConvention computeCallingConvention(sema::Function const& function);

/// Translates the function declaration \p semaFn to an IR function.
/// \Note This does not generate code
ir::Callable* declareFunction(sema::Function const& semaFn,
                              LoweringContext lctx);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_GLOBALDECLS_H_
