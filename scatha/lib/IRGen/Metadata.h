#ifndef SCATHA_IRGEN_METADATA_H_
#define SCATHA_IRGEN_METADATA_H_

#include <utl/hashtable.hpp>
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

    /// Type of the vtable
    ir::StructType* vtableType = nullptr;

    /// The global vtable constant
    ir::GlobalVariable* vtable = nullptr;

    /// Maps sema functions to their index in the vtable
    utl::hashmap<sema::Function const*, size_t> vtableIndexMap;
};

/// Metadata for translating a function from Scatha to IR
struct FunctionMetadata {
    /// The IR function
    ir::Callable* function = nullptr;

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
