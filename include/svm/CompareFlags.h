#ifndef SVM_COMPAREFLAGS_H_
#define SVM_COMPAREFLAGS_H_

namespace svm {

/// Internal VM flags that store the result of the last compare instruction
struct CompareFlags {
    bool less  : 1;
    bool equal : 1;
};

} // namespace svm

#endif // SVM_COMPAREFLAGS_H_
