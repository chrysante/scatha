// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_VM_INSTRUCTION_H_
#define SCATHA_VM_INSTRUCTION_H_

#include <scatha/Basic/Basic.h>

namespace scatha::vm {

class VirtualMachine;

using Instruction = u64 (*)(u8 const*, u64*, class VirtualMachine*);

} // namespace scatha::vm

#endif // SCATHA_VM_INSTRUCTION_H_
