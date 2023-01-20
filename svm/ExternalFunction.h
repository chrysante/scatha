#ifndef SVM_EXTERNALFUNCTION_H_
#define SVM_EXTERNALFUNCTION_H_

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

using ExternalFunction = void (*)(u64* regPtr, VirtualMachine*);

} // namespace svm

#endif // SVM_EXTERNALFUNCTION_H_
