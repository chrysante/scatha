#ifndef SCATHA_IR_PASSMANAGER_H_
#define SCATHA_IR_PASSMANAGER_H_

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/IR/Pass.h>
#include <scatha/IR/Pipeline.h>

namespace scatha::ir {

/// Static pass manager interface.
/// Optimization passes can be accessed dynamically via the interface.
/// Passes are registered at static initialization phase.
class SCATHA_API PassManager {
public:
    /// Get the loop transform pass with name \p name
    static LoopPass getLoopPass(std::string_view name);

    /// Get the function transform pass with name \p name
    static FunctionPass getFunctionPass(std::string_view name);

    /// Get the module transform pass with name \p name
    static ModulePass getModulePass(std::string_view name);

    /// Make a pipeline from the pipeline script \p script
    static Pipeline makePipeline(std::string_view script);

    /// \Returns A list of all local passes
    static std::vector<FunctionPass> functionPasses();

    /// \Returns A list of all function passes of category \p category
    static std::vector<FunctionPass> functionPasses(PassCategory category);
};

} // namespace scatha::ir

#endif // SCATHA_IR_PASSMANAGER_H_
