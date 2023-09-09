#ifndef SCATHA_IRGEN_METADATA_H_
#define SCATHA_IRGEN_METADATA_H_

#include <utl/vector.hpp>

#include "IRGen/CallingConvention.h"

namespace scatha::irgen {

///
struct StructMetaData {
    ///
    utl::small_vector<uint16_t> indexMap;
};

///
struct FunctionMetaData {
    ///
    CallingConvention CC;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_METADATA_H_
