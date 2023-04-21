#ifndef SCATHA_COMPILER_COMPILER_H_
#define SCATHA_COMPILER_COMPILER_H_

#include <filesystem>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

namespace scatha {

struct CompilerOptions {
    int optimizationLevel  = 0;
    bool associativeFloats = false;
};

SCATHA_API utl::vector<uint8_t> compile(
    utl::vector<std::filesystem::path> const& inputFiles,
    CompilerOptions options);

} // namespace scatha

#endif // SCATHA_COMPILER_COMPILER_H_
