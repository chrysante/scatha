#ifndef SVM_INSTRUCTION_H_
#define SVM_INSTRUCTION_H_

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

using Instruction = u64 (*)(u8 const*, u64*, class VirtualMachine*);

} // namespace svm

#endif // SVM_INSTRUCTION_H_
