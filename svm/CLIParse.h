#ifndef SVM_CLIPARSE_H_
#define SVM_CLIPARSE_H_

#include <filesystem>

namespace svm {

struct Options {
    std::filesystem::path filepath;
    bool time;
};

Options parseCLI(int argc, char* argv[]);

} // namespace svm

#endif // SVM_CLIPARSE_H_