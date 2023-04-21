#ifndef SVM_BUILTININTERNAL_H_
#define SVM_BUILTININTERNAL_H_

#include <svm/Builtin.h>
#include <utl/vector.hpp>

namespace svm {

utl::vector<ExternalFunction> makeBuiltinTable();

} // namespace svm

#endif // SVM_BUILTININTERNAL_H_
