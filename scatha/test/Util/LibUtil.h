#ifndef SCATHA_TEST_LIBUTIL_H_
#define SCATHA_TEST_LIBUTIL_H_

#include <filesystem>
#include <string>

namespace scatha::test {

/// Compiles the text \p source into a library and emits it as \p name
void compileLibrary(std::filesystem::path name,
                    std::filesystem::path libSearchPath, std::string source);

/// Compiles and runs the program \p source that depends on libraries in \p
/// libSearchPath
uint64_t compileAndRunDependentProgram(std::filesystem::path libSearchPath,
                                       std::string source);

} // namespace scatha::test

#endif // SCATHA_TEST_LIBUTIL_H_
