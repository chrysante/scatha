#ifndef SCATHA_INVOCATION_COMPILERINVOCATION_H_
#define SCATHA_INVOCATION_COMPILERINVOCATION_H_

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <scatha/AST/Fwd.h>
#include <scatha/Assembly/Fwd.h>
#include <scatha/CodeGen/Logger.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Invocation/Target.h>
#include <scatha/Sema/Fwd.h>

namespace scatha {

/// Different compiler frontends
enum class FrontendType { Scatha, IR };

/// Compiler stage callbacks provide hooks to customize the compilation process
struct CompilerCallbacks {
    /// This will be invoked after parsing and semantic analysis if the compiler
    /// is invoked in "Scatha" mode
    std::function<void(ast::ASTNode&, sema::SymbolTable&)> frontendCallback;

    /// This function will be invoked after IR generation in "Scatha" mode or
    /// after parsing in "IR" mode
    std::function<void(ir::Context&, ir::Module&)> irgenCallback;

    /// This function will be invoked after IR optimization passes
    std::function<void(ir::Context&, ir::Module&)> optCallback;

    /// This function will be invoked after code generation
    std::function<void(Asm::AssemblyStream&)> codegenCallback;

    /// This function will be invoked after the assembler has run
    std::function<void(Asm::AssemblerResult const&)> asmCallback;

    /// This function will be invoked after the linker has run
    std::function<void(std::span<uint8_t const> program)> linkerCallback;
};

/// Represents one invocation of the compiler
class SCATHA_API CompilerInvocation {
public:
    /// \param targetType The type of the target that is to be compiled
    /// \param name The name of the target
    CompilerInvocation(TargetType targetType, std::string name);

    /// Set the source texts to be compiled to \p sources
    void setInputs(std::vector<SourceFile> sources);

    /// Set the paths to be searched for library imports to \p directories
    void setLibSearchPaths(std::vector<std::filesystem::path> directories);

    /// Set the compiler stage callbacks to \p callbacks
    void setCallbacks(CompilerCallbacks callbacks);

    /// Sets the frontend to \p frontend
    /// Defaults to `FrontendType::Scatha`
    void setFrontend(FrontendType frontend) { this->frontend = frontend; }

    /// Sets the optimization level to \p level
    /// Defaults to 0
    void setOptLevel(int level) { optLevel = level; }

    /// Sets the optimization pipeline to \p pipeline
    /// \Note this option is ignored unless the optimization level is 0
    /// Defaults to empty
    void setOptPipeline(std::string pipeline) {
        optPipeline = std::move(pipeline);
    }

    /// Sets the codegen logger to \p logger
    /// Defaults to an instance of `cg::NullLogger`
    void setCodegenLogger(cg::Logger& logger) { codegenLogger = &logger; }

    /// Tell the compiler whether debug info files shall be generated
    void generateDebugInfo(bool value = true) { genDebugInfo = value; }

    /// Set the ostream to write errors to to \p ostream
    /// Defaults to `std::cout`
    void setErrorStream(std::ostream& ostream) { errStream = &ostream; }

    /// Sets the error handler to \p handler
    void setErrorHandler(std::function<void()> handler) {
        errorHandler = std::move(handler);
    }

    /// Stops the compilation process. This function is meant to be called from
    /// a callback to stop compilation early
    void stop() { continueCompilation = false; }

    /// Invoke the compiler with the given options
    /// \Returns the compiled target if no errors occurred and compilation was
    /// not stopped
    std::optional<Target> run();

private:
    std::ostream& err() { return *errStream; }

    void handleError();

    TargetType targetType;
    std::string name;
    std::vector<SourceFile> sources;
    std::vector<std::filesystem::path> libSearchPaths;
    CompilerCallbacks callbacks;
    std::function<void()> errorHandler;
    std::string optPipeline = {};
    std::ostream* errStream;
    cg::Logger* codegenLogger = nullptr;
    int optLevel = 0;
    FrontendType frontend = FrontendType::Scatha;
    bool genDebugInfo = false;
    bool continueCompilation = true;
};

} // namespace scatha

#endif // SCATHA_INVOCATION_COMPILERINVOCATION_H_
