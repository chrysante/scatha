#ifndef SCATHAC_INSPECT_H_
#define SCATHAC_INSPECT_H_

#include <optional>

#include "Options.h"

namespace scatha {

/// Command line options for the `inspect` debug tool
struct InspectOptions: BaseOptions {
    bool ast;
    bool sym;
    bool printBuiltins;
    bool expsym;
    bool emitIR;
    bool codegen;
    bool isel; // Experimental
    bool assembly;
    bool onlyFrontend;
};

/// Main function of the `inspect` tool
int inspectMain(InspectOptions options);

} // namespace scatha

#endif // SCATHAC_INSPECT_H_
