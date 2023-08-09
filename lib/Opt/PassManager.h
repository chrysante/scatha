#ifndef SCATHA_OPT_PASSMANAGER_H_
#define SCATHA_OPT_PASSMANAGER_H_

#include <functional>
#include <string>
#include <string_view>

#include <utl/vector.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"
#include "Opt/Pass.h"
#include "Opt/Pipeline.h"

namespace scatha::opt {

/// Static pass manager interface
/// Passes are registered at static initialization phase
class SCATHA_API PassManager {
public:
    using PassType = bool (*)(ir::Context&, ir::Function&);

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

/// Register a local or global pass
#define SC_REGISTER_PASS(function, name)                                       \
    static int SC_CONCAT(__Pass_, __COUNTER__) = [] {                          \
        ::scatha::opt::internal::registerPass({ function, name });             \
        return 0;                                                              \
    }()

/// Register a global pass. Same as `SC_REGISTER_PASS` except that \p function
/// is cast to global pass signature
#define SC_REGISTER_GLOBAL_PASS(function, name)                                \
    SC_REGISTER_PASS(                                                          \
        static_cast<bool (*)(ir::Context&, ir::Module&, LocalPass)>(function), \
        name)

namespace scatha::opt::internal {

void registerPass(LocalPass pass);

void registerPass(GlobalPass pass);

} // namespace scatha::opt::internal

#endif // SCATHA_OPT_PASSMANAGER_H_
