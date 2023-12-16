#ifndef SCATHAC_INSPECT_H_
#define SCATHAC_INSPECT_H_

#include "Options.h"

namespace scatha {

/// Command line options for the `inspect` debug tool
struct InspectOptions: OptionsBase {
    bool ast;
    bool sym;
    bool emitIR;
    bool codegen;
    bool isel; // Experimental
    bool assembly;
    std::optional<std::filesystem::path> out;
};

/// Main function of the `inspect` tool
int inspectMain(InspectOptions options);

} // namespace scatha

#endif // SCATHAC_INSPECT_H_
