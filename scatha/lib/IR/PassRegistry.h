#ifndef SCATHA_IR_PASSREGISTRY_H_
#define SCATHA_IR_PASSREGISTRY_H_

#include "Common/Base.h"
#include "IR/Pass.h"

#define _SC_REGISTER_PASS_IMPL(impl, function, name, category, ...)            \
    static int SC_CONCAT(__Pass_, __COUNTER__) = [] {                          \
        using namespace ::scatha::ir::passParameterTypes;                      \
        ::scatha::ir::internal::impl(                                          \
            { function, ::scatha::ir::PassArgumentMap __VA_ARGS__, name,       \
              category });                                                     \
        return 0;                                                              \
    }()

/// Register a function pass
#define SC_REGISTER_FUNCTION_PASS(function, name, category, ...)               \
    _SC_REGISTER_PASS_IMPL(registerFunctionPass, function, name, category,     \
                           __VA_ARGS__)

/// Register a global pass. Same as `SC_REGISTER_FUNCTION_PASS` except that \p
/// function is cast to module pass signature
#define SC_REGISTER_MODULE_PASS(function, name, category, ...)                 \
    _SC_REGISTER_PASS_IMPL(                                                    \
        registerModulePass,                                                    \
        static_cast<bool (*)(::scatha::ir::Context&, ::scatha::ir::Module&,    \
                             ::scatha::ir::FunctionPass,                       \
                             ::scatha::ir::PassArgumentMap const&)>(function), \
        name, category, __VA_ARGS__)

namespace scatha::ir::internal {

void registerFunctionPass(FunctionPass pass);

void registerModulePass(ModulePass pass);

} // namespace scatha::ir::internal

#endif // SCATHA_IR_PASSREGISTRY_H_
