#ifndef SCATHA_IR_PASSREGISTRY_H_
#define SCATHA_IR_PASSREGISTRY_H_

#include "Common/Base.h"
#include "IR/Pass.h"

#define _SC_REGISTER_PASS_IMPL(impl, function, name, category, ...)            \
    static int SC_CONCAT(__Pass_, __COUNTER__) = [] {                          \
        ::scatha::ir::internal::impl(                                          \
            { function, ::scatha::ir::PassArgumentMap __VA_ARGS__, name,       \
              category });                                                     \
        return 0;                                                              \
    }()

/// Register a local pass
#define SC_REGISTER_PASS(function, name, category, ...)                        \
    _SC_REGISTER_PASS_IMPL(registerLocal, function, name, category, __VA_ARGS__)

/// Register a global pass. Same as `SC_REGISTER_PASS` except that \p function
/// is cast to global pass signature
#define SC_REGISTER_GLOBAL_PASS(function, name, category, ...)                 \
    _SC_REGISTER_PASS_IMPL(registerGlobal,                                     \
                           static_cast<bool (*)(ir::Context&, ir::Module&,     \
                                                LocalPass)>(function),         \
                           name, category, __VA_ARGS__)

namespace scatha::ir::internal {

void registerLocal(LocalPass pass);

void registerGlobal(GlobalPass pass);

} // namespace scatha::ir::internal

#endif // SCATHA_IR_PASSREGISTRY_H_
