#ifndef SCATHA_TEST_IR_COMPILETOIR_H_
#define SCATHA_TEST_IR_COMPILETOIR_H_

#include <string_view>

#include "IR/Module.h"

namespace scatha::test {

ir::Module compileToIR(std::string_view text);

} // namespace scatha::test


#endif // SCATHA_TEST_IR_COMPILETOIR_H_
