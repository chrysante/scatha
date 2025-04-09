#include "svm/OpCode.h"

#include <array>
#include <ostream>

using namespace svm;

std::string_view svm::toString(OpCode c) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) std::string_view(#inst),
#include <svm/OpCode.def.h>
    }[static_cast<size_t>(c)];
}

std::ostream& svm::operator<<(std::ostream& str, OpCode c) {
    return str << toString(c);
}
