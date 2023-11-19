#ifndef SVM_BUILTININTERNAL_H_
#define SVM_BUILTININTERNAL_H_

#include <vector>

#include <svm/Builtin.h>

namespace svm {

std::vector<ExternalFunction> makeBuiltinTable();

} // namespace svm

#endif // SVM_BUILTININTERNAL_H_
