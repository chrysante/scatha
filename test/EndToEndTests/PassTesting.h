#ifndef SCATHA_TEST_ENDTOENDTESTS_PASSTESTING_H_
#define SCATHA_TEST_ENDTOENDTESTS_PASSTESTING_H_

#include <optional>
#include <span>
#include <string_view>

#include <utl/function_view.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::test {

/// Runs the CLI specified test cases and checks for return value
void runReturnsTest(u64 value, std::vector<std::string> sourceTexts);

/// \overload for single string
inline void runReturnsTest(u64 value, std::string text) {
    runReturnsTest(value, std::vector{ std::move(text) });
}

/// Runs the CLI specified test cases and checks for return value
/// \p text is interpreted as IR
void runIRReturnsTest(u64 value, std::string_view text);

/// \Returns `true` if \p text compiles successfully
[[nodiscard]] bool compiles(std::string text);

/// \Returns `true` if \p text compiles successfully
/// \p text is interpreted as IR
[[nodiscard]] bool IRCompiles(std::string_view text);

/// Compiles the given source code and discards the result
void compile(std::string text);

/// Runs the given binary and returns the result
uint64_t runProgram(std::span<uint8_t const> program, size_t startpos = 0);

/// \Returns the address of the main function if it exists in the symbol table
/// \p symbolTable
std::optional<size_t> findMain(
    std::unordered_map<std::string, size_t> const& symbolTable);

/// Compiles and runs the given source code and returns the result
uint64_t compileAndRun(std::string text);

/// Compiles and runs the given source code as IR and returns the result
uint64_t compileAndRunIR(std::string text);

/// Runs the CLI specified test cases and checks for text printed to stdout
void runPrintsTest(std::string_view printed, std::string source);

/// Runs the CLI specified test cases and checks for text printed to stdout
/// \p text is interpreted as IR
void runIRPrintsTest(std::string_view printed, std::string source);

} // namespace scatha::test

#endif // SCATHA_TEST_ENDTOENDTESTS_PASSTESTING_H_
