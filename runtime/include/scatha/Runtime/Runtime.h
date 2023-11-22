#ifndef SCATHA_RUNTIME_RUNTIME_H_
#define SCATHA_RUNTIME_RUNTIME_H_

#include <scatha/Runtime/Compiler.h>
#include <scatha/Runtime/Executor.h>
#include <scatha/Runtime/Support.h>

namespace scatha {

class Runtime {
public:
    ///
    bool declareFunction(std::string name,
                         sema::FunctionSignature signature,
                         InternalFuncPtr impl,
                         void* userptr);

    ///
    template <ValidFunction F>
    bool declareFunction(std::string name, F&& f);

    ///
    bool compile();

private:
    Compiler comp;
    Executor exec;
};

} // namespace scatha

// ========================================================================== //
// ===  Inline implementation  ============================================== //
// ========================================================================== //

template <scatha::ValidFunction F>
bool scatha::Runtime::declareFunction(std::string name, F&& f) {
    auto [impl, userptr] = internal::makeImplAndUserPtr(std::forward<F>(f));
    return declareFunction(std::move(name),
                           comp.extractSignature<F>(),
                           impl,
                           userptr);
}

#endif // SCATHA_RUNTIME_RUNTIME_H_
