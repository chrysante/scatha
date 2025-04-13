#include "scbinutil/OpCode.h"

#include <array>
#include <ostream>
#include <stdexcept>

using namespace scbinutil;

std::string_view scbinutil::toString(OpCode c) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) std::string_view(#inst),
#include <scbinutil/OpCode.def.h>
    }[static_cast<size_t>(c)];
}

std::ostream& scbinutil::operator<<(std::ostream& str, OpCode c) {
    return str << toString(c);
}

void internal::throwRuntimeError(std::string_view msg) {
    throw std::runtime_error(std::string(msg));
}
