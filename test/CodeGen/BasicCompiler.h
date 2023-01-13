#ifndef SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
#define SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

#include <string_view>

#include "Basic/Basic.h"

namespace scatha::test {

void checkReturns(u64 value, std::string_view text);

} // namespace scatha::test

#endif // SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
