#ifndef SCATHA_INVOCATION_COMPILERINVOCATION_H_
#define SCATHA_INVOCATION_COMPILERINVOCATION_H_

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Sema/Fwd.h>

namespace scatha {

/// Different types of targets the compiler can generate
/// - `Executable` Generates a binary program and makes the output file
/// executable on the system
/// - `BinaryOnly` Generates a binary program that can not be executed directly
/// - `StaticLibrary` Generates a static library
enum class TargetType { Executable, BinaryOnly, StaticLibrary };

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
    std::function<void()> codegenCallback;

    /// This function will be invoked after the assembler has run
    std::function<void()> asmCallback;

    /// This function will be invoked after the linker has run
    std::function<void()> linkerCallback;
};

/// Represents one invocation of the compiler
class CompilerInvocation {
public:
    ///
    CompilerInvocation();

    /// Set the source texts to be compiled to \p sources
    void setInputs(std::vector<SourceFile> sources);

    /// Set the paths to be searched for library imports to \p directories
    void setLibSearchPaths(std::vector<std::filesystem::path> directories);

    /// Set the compiler stage callbacks to \p callbacks
    void setCallbacks(CompilerCallbacks callbacks);

    /// Sets the target type to \p type
    /// Defaults to `TargetType::Executable`
    TargetType setTargetType(TargetType type) { targetType = type; }

    /// Sets the frontend to \p frontend
    /// Defaults to `FrontendType::Scatha`
    void setFrontend(FrontendType frontend) { this->frontend = frontend; }

    /// Sets the output file to \p file
    /// Defaults to "out"
    void setOutputFile(std::filesystem::path file) {
        outputFile = std::move(file);
    }

    /// Sets the optimization level to \p level
    /// Defaults to 0
    void setOptLevel(int level) { optLevel = level; }

    /// Sets the optimization pipeline to \p pipeline
    /// \Note this option is ignored unless the optimization level is 0
    /// Defaults to empty
    void setOptPipeline(std::string pipeline) {
        optPipeline = std::move(pipeline);
    }

    /// Tell the compiler whether debug info files shall be generated
    void generateDebugInfo(bool value = true) { genDebugInfo = value; }

    /// Set the ostream to write errors to to \p ostream
    /// Defaults to `std::cout`
    void setErrorStream(std::ostream& ostream);

    /// Stops the compilation process. This function is meant to be called from
    /// a callback to stop compilation early
    void stop() { continueCompilation = false; }

    /// Invoke the compiler with the given options
    int run();

private:
    std::ostream& err() { return *errStream; }

    std::vector<SourceFile> sources;
    std::vector<std::filesystem::path> libSearchPaths;
    CompilerCallbacks callbacks;
    std::filesystem::path outputFile = "out";
    std::string optPipeline = {};
    std::ostream* errStream;
    int optLevel = 0;
    TargetType targetType = TargetType::Executable;
    FrontendType frontend = FrontendType::Scatha;
    bool genDebugInfo = false;
    bool continueCompilation = true;
};

} // namespace scatha

#endif // SCATHA_INVOCATION_COMPILERINVOCATION_H_
