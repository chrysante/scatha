#ifndef SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
#define SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

#include <string_view>

#include <utl/function_view.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::test {

void checkReturns(u64 value, std::string_view text);

void checkIRReturns(u64 value, std::string_view text);

void checkIRReturns(
    u64 value,
    std::string_view text,
    utl::function_view<void(ir::Context&, ir::Module&)> optFunction);

} // namespace scatha::test

#endif // SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
