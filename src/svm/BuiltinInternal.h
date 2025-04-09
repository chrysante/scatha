#ifndef SVM_BUILTININTERNAL_H_
#define SVM_BUILTININTERNAL_H_

#include <vector>

#include <svm/Builtin.h>

#include "ExternalFunction.h"

namespace svm {

std::vector<BuiltinFunction> makeBuiltinTable();

} // namespace svm

#endif // SVM_BUILTININTERNAL_H_
