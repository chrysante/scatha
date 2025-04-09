#ifndef SCATHAC_COMPILER_H_
#define SCATHAC_COMPILER_H_

#include <filesystem>

#include "Options.h"

namespace scatha {

/// Command line options of the user facing compiler
struct CompilerOptions: OptionsBase {
    /// Set if time taken by compilation shall be printed
    bool time;

    /// Set if debug symbols shall be generated
    bool debug;
};

/// User facing compiler main function
int compilerMain(CompilerOptions options);

} // namespace scatha

#endif // SCATHAC_COMPILER_H_
