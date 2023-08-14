#ifndef SCATHA_OPT_PASSREGISTRY_H_
#define SCATHA_OPT_PASSREGISTRY_H_

#include "Common/Base.h"
#include "Opt/Pass.h"

#define _SC_REGISTER_PASS_IMPL(impl, function, name)                           \
    static int SC_CONCAT(__Pass_, __COUNTER__) = [] {                          \
        ::scatha::opt::internal::impl({ function, name });                     \
        return 0;                                                              \
    }()

/// Register a local pass
#define SC_REGISTER_PASS(function, name)                                       \
    _SC_REGISTER_PASS_IMPL(registerLocal, function, name)

/// Register a canonicalization pass
#define SC_REGISTER_CANONICALIZATION(function, name)                           \
    _SC_REGISTER_PASS_IMPL(registerCanonicalization, function, name)

/// Register a global pass. Same as `SC_REGISTER_PASS` except that \p function
/// is cast to global pass signature
#define SC_REGISTER_GLOBAL_PASS(function, name)                                \
    _SC_REGISTER_PASS_IMPL(                                                    \
        registerGlobal,                                                        \
        static_cast<bool (*)(ir::Context&, ir::Module&, LocalPass)>(function), \
        name)

namespace scatha::opt::internal {

void registerLocal(LocalPass pass);

void registerCanonicalization(LocalPass pass);

void registerGlobal(GlobalPass pass);

} // namespace scatha::opt::internal

#endif // SCATHA_OPT_PASSREGISTRY_H_
