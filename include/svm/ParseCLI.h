#ifndef SVM_PARSECLI_H_
#define SVM_PARSECLI_H_

#include <filesystem>
#include <vector>

namespace svm {

///
struct Options {
    std::filesystem::path filepath;
    std::vector<std::string> arguments;
    bool time;
};

///
Options parseCLI(int argc, char* argv[]);

} // namespace svm

#endif // SVM_PARSECLI_H_
