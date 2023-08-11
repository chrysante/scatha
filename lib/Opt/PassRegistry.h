#ifndef SCATHA_OPT_PASSMANAGER_IMPL_H_
#define SCATHA_OPT_PASSMANAGER_IMPL_H_

#include "Common/Base.h"
#include "Opt/Pass.h"

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

#endif // SCATHA_OPT_PASSMANAGER_IMPL_H_
