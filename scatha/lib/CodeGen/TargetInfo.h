#ifndef SCATHA_CODEGEN_TARGETINFO_H_
#define SCATHA_CODEGEN_TARGETINFO_H_

#include "Common/Base.h"

namespace scatha::cg {

/// \Returns the number of registers a native call instructions requires below
/// the register offset to store call metadata such as return address and stack
/// pointer
size_t numRegistersForCallMetadata();

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_TARGETINFO_H_
