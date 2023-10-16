#ifndef SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
#define SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

#include <string_view>

#include <utl/function_view.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::test {

void checkReturns(u64 value, std::vector<std::string> sourceTexts);

void checkReturns(u64 value, auto... text) {
    checkReturns(value, std::vector<std::string>{ std::move(text)... });
}

void checkIRReturns(u64 value, std::string_view text);

void checkCompiles(std::string text);

void checkIRCompiles(std::string_view text);

void compileAndRun(std::string text);

void compileAndRunIR(std::string text);

void checkPrints(std::string_view printed, std::string source);

void checkIRPrints(std::string_view printed, std::string source);

} // namespace scatha::test

#endif // SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
