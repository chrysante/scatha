#ifndef SCATHA_RUNTIME_COMPILER_H_
#define SCATHA_RUNTIME_COMPILER_H_

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <iosfwd>

#include <scatha/Runtime/Common.h>
#include <scatha/Runtime/Program.h>

namespace scatha {

class IssueHandler;

/// Wraps the internal compilation interface for integration in a host environment
class Compiler {
public:
    explicit Compiler();

    Compiler(Compiler&&) noexcept;

    Compiler& operator=(Compiler&&) noexcept;

    ~Compiler();

    /// Declare an external function
    /// If the program calls a non-builtin external function, it must be declared prior to compilation.
    /// This method wraps a call to the internal symbol table if the semantic analysis phase
    std::optional<ExtFunctionID> declareFunction(
        std::string name,
        QualType returnType,
        std::span<QualType const> argTypes);

    /// \overload
    std::optional<ExtFunctionID> declareFunction(
        std::string name,
        QualType returnType,
        std::initializer_list<QualType> argTypes) {
        return declareFunction(std::move(name),
                               returnType,
                               std::span<QualType const>(argTypes));
    }

    /// Add a source file of the to-be-compiled program
    void addSource(std::string source) { sources.push_back(std::move(source)); }

    /// \Returns `nullptr` if compilation failed.
    std::unique_ptr<Program> compile(IssueHandler& issueHandler);

    /// \overload
    /// Issues are printed to \p ostream
    std::unique_ptr<Program> compile(std::ostream& ostream);
    
    /// \overload
    /// Issues are printed to `std::cout`
    std::unique_ptr<Program> compile();
    
private:
    struct Impl;

    std::vector<std::string> sources;
    std::unique_ptr<Impl> impl;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_RUNTIME_H_
