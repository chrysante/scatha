#ifndef SCATHA_RUNTIME_RUNTIME_H_
#define SCATHA_RUNTIME_RUNTIME_H_

#include <scatha/Runtime/Compiler.h>
#include <scatha/Runtime/Executor.h>
#include <scatha/Runtime/Support.h>

namespace scatha {

class Runtime {
public:
    Runtime();

    ///
    bool addFunction(std::string name, sema::FunctionType const* type,
                     InternalFuncPtr impl, void* userptr);

    ///
    template <ValidFunction F>
    bool addFunction(std::string name, F&& f);

    /// Adds source code from memory
    void addSourceText(std::string text, std::filesystem::path path = {}) {
        comp.addSourceText(std::move(text), std::move(path));
    }

    /// Loads source code from file
    void addSourceFile(std::filesystem::path path) {
        comp.addSourceFile(std::move(path));
    }

    ///
    bool compile();

    /// \Returns a `std::optional` function object with call signature `Sig`
    template <typename Sig>
    auto getFunction(std::string name);

private:
    Compiler comp;
    std::unique_ptr<Executor> exec;
};

} // namespace scatha

// ========================================================================== //
// ===  Inline implementation  ============================================== //
// ========================================================================== //

template <scatha::ValidFunction F>
bool scatha::Runtime::addFunction(std::string name, F&& f) {
    auto [impl, userptr] = internal::makeImplAndUserPtr(std::forward<F>(f));
    return addFunction(std::move(name), comp.extractSignature<F>(), impl,
                       userptr);
}

template <typename Sig>
auto scatha::Runtime::getFunction(std::string name) {
    return exec->getFunction<Sig>(std::move(name));
}

#endif // SCATHA_RUNTIME_RUNTIME_H_
