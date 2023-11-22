#ifndef SCATHA_RUNTIME_EXECUTOR_H_
#define SCATHA_RUNTIME_EXECUTOR_H_

#include <memory>

#include <scatha/Runtime/Program.h>
#include <scatha/Runtime/Support.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

namespace scatha {

class Program;

///
class Executor {
public:
    /// Create an empty executor
    static std::unique_ptr<Executor> Make();

    /// Create an executor from a program
    static std::unique_ptr<Executor> Make(Program program);

    /// Loads the program \p program into this executor
    void load(Program program);

    /// Defines the function declaration \p decl as \p impl
    void addFunction(FuncDecl decl, InternalFuncPtr impl, void* userptr);

    /// Defines the function declaration \p decl as \p impl
    template <ValidFunction F>
    void addFunction(FuncDecl decl, F&& impl);

    /// \Returns a `std::optional` function object with call signature `Sig`
    template <typename Sig>
    auto getFunction(std::string name);

private:
    Executor() = default;
    Executor(Program);
    Executor(Executor const&) = delete;
    Executor& operator=(Executor const&) = delete;

    svm::VirtualMachine vm;
    Program prog;
};

} // namespace scatha

// ========================================================================== //
// ===  Inline implementation  ============================================== //
// ========================================================================== //

template <scatha::ValidFunction F>
void scatha::Executor::addFunction(FuncDecl decl, F&& f) {
    auto [impl, userptr] = internal::makeImplAndUserPtr(std::forward<F>(f));
    addFunction(std::move(decl), impl, userptr);
}

template <typename Sig>
auto scatha::Executor::getFunction(std::string name) {
    using ResultType =
        decltype(internal::MakeFunction<Sig>::Impl(&vm, size_t{}));
    auto addr = prog.getAddress(std::move(name));
    if (addr) {
        return std::optional{ internal::MakeFunction<Sig>::Impl(&vm, *addr) };
    }
    return std::optional<ResultType>{};
}

#endif // SCATHA_RUNTIME_EXECUTOR_H_
