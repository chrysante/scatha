#ifndef SCATHA_OPT_PASSMANAGER_H_
#define SCATHA_OPT_PASSMANAGER_H_

#include <functional>
#include <string>
#include <string_view>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Opt/Pass.h>
#include <scatha/Opt/Pipeline.h>

namespace scatha::opt {

/// Static pass manager interface.
/// Optimization passes can be accessed dynamically via the interface.
/// Passes are registered at static initialization phase.
class SCATHA_API PassManager {
public:
    /// Get the local transform pass with name \p name
    static LocalPass getPass(std::string_view name);

    /// Get the global transform pass with name \p name
    static GlobalPass getGlobalPass(std::string_view name);

    /// Make a pipeline from the pipeline script \p script
    static Pipeline makePipeline(std::string_view script);

    /// \Returns A list of all local passes
    static utl::vector<LocalPass> localPasses();
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PASSMANAGER_H_
