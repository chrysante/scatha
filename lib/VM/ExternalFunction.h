// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_VM_EXTERNALFUNCTION_H_
#define SCATHA_VM_EXTERNALFUNCTION_H_

#include <scatha/Basic/Basic.h>

namespace scatha::vm {

class VirtualMachine;

using ExternalFunction = void (*)(u64* regPtr, VirtualMachine*);

} // namespace scatha::vm

#endif // SCATHA_VM_EXTERNALFUNCTION_H_
