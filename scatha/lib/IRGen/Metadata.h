#ifndef SCATHA_IRGEN_METADATA_H_
#define SCATHA_IRGEN_METADATA_H_

#include <utl/vector.hpp>

#include "IRGen/CallingConvention.h"

namespace scatha::irgen {

/// Metadata for translation of a struct member from Scatha to IR
struct MemberMetadata {
    /// The index at which the field that represent this data member begin
    size_t beginIndex;

    /// The field types representing this data member in IR
    utl::small_vector<ir::Type const*, 2> fieldTypes;
};

/// Metadata for translation of a struct from Scatha to IR
struct RecordMetadata {
    /// Metadata objects for each member in the Scatha representation of the
    /// struct
    utl::small_vector<MemberMetadata> members;

    ///
    ir::GlobalVariable* vtable = nullptr;
};

/// Metadata for translating a function from Scatha to IR
struct FunctionMetadata {
    /// The IR function
    ir::Callable* function;

    /// The corresponding calling convention
    CallingConvention CC;
};

/// Metadata for translating a global variable from Scatha to IR
struct GlobalVarMetadata {
    /// The IR global variable object
    ir::GlobalVariable* var;

    /// The global flag that tells if the variable is initialized
    ir::GlobalVariable* varInit;

    /// The function that retrieves the pointer to the variable
    ir::Function* getter;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_METADATA_H_
