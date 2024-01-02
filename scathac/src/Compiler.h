#ifndef SCATHAC_COMPILER_H_
#define SCATHAC_COMPILER_H_

#include <filesystem>

#include "Options.h"

namespace scatha {

/// Command line options of the user facing compiler
struct CompilerOptions: OptionsBase {
    /// Output directory
    std::filesystem::path bindir = "out";

    /// Set if time taken by compilation shall be printed
    bool time;

    /// Turn optimizations on level 1
    bool optimize;

    /// Only emit the binary without making it executable
    bool binaryOnly;

    /// Set if debug symbols shall be generated
    bool debug;
};

/// User facing compiler main function
int compilerMain(CompilerOptions options);

} // namespace scatha

#endif // SCATHAC_COMPILER_H_
