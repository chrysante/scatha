#include "Invocation/CompilerInvocation.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/format_time.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "Common/ExecutableWriter.h"
#include "Common/SourceFile.h"
#include "Common/UniquePtr.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PassManager.h"
#include "IR/Print.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Opt/Optimizer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/NameMangling.h"
#include "Sema/Serialize.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

static const utl::vstreammanip<> Warning = [](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
};

static const utl::vstreammanip<> Error = [](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
};

CompilerInvocation::CompilerInvocation(): errStream(&std::cout) {}

void CompilerInvocation::setInputs(std::vector<SourceFile> sources) {
    this->sources = std::move(sources);
}

void CompilerInvocation::setLibSearchPaths(
    std::vector<std::filesystem::path> directories) {
    libSearchPaths = std::move(directories);
}

void CompilerInvocation::setCallbacks(CompilerCallbacks callbacks) {
    this->callbacks = std::move(callbacks);
}

/// Tries to invoke the callback with \p args...
template <typename... T, typename... Args>
static void tryInvoke(std::function<void(T...)> const& f, Args&&... args) {
    if (f) {
        f(std::forward<Args>(args)...);
    }
}

static std::filesystem::path appendExt(std::filesystem::path p,
                                       std::string_view ext) {
    p += ".";
    p += ext;
    return p;
}

static std::fstream createFile(std::filesystem::path const& path,
                               std::ios::openmode flags = {}) {

    if (auto parent = path.parent_path();
        !parent.empty() && !std::filesystem::exists(parent))
    {
        std::filesystem::create_directories(parent);
    }
    flags |= std::ios::out | std::ios::trunc;
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to create ", path));
    }
    return file;
}

static void printLinkerError(Asm::LinkerError const& error, std::ostream& str) {
    str << Error << "Linker failed to resolve symbol references:\n";
    for (auto& symbol: error.missingSymbols) {
        str << "  - " << symbol << "\n";
    }
}

int CompilerInvocation::run() {
    using enum TargetType;
    /// Mode validation
    if (frontend == FrontendType::IR) {
        if (targetType != Executable && targetType != BinaryOnly) {
            err() << Error << "Can only generate executables with IR frontend"
                  << std::endl;
            return handleError();
        }
    }
    if (sources.empty()) {
        err() << Error << "No input files" << std::endl;
        return handleError();
    }
    /// Now we compile the program
    sema::SymbolTable semaSym;
    ir::Context context;
    ir::Module mod;
    switch (frontend) {
    case FrontendType::Scatha: {
        IssueHandler issueHandler;
        auto ast = parser::parse(sources, issueHandler);
        if (!issueHandler.empty()) {
            issueHandler.print(sources);
        }
        if (!ast) {
            handleError();
            return handleError();
        }
        auto analysisResult =
            sema::analyze(*ast,
                          semaSym,
                          issueHandler,
                          { .librarySearchPaths = libSearchPaths });
        if (!issueHandler.empty()) {
            issueHandler.print(sources, err());
        }
        if (issueHandler.haveErrors()) {
            return handleError();
        }
        tryInvoke(callbacks.frontendCallback, *ast, semaSym);
        if (!continueCompilation) return 0;
        irgen::Config irgenConfig = { .sourceFiles = sources,
                                      .generateDebugSymbols = genDebugInfo };
        if (targetType == TargetType::StaticLibrary) {
            irgenConfig.nameMangler = sema::NameMangler(
                { .globalPrefix = outputFile.stem().string() });
        }
        irgen::generateIR(context,
                          mod,
                          *ast,
                          semaSym,
                          analysisResult,
                          std::move(irgenConfig));
        tryInvoke(callbacks.irgenCallback, context, mod);
        if (!continueCompilation) return 0;
        break;
    }
    case FrontendType::IR: {
        if (sources.size() != 1) {
            err() << Error << "Can only parse one IR file" << std::endl;
            return handleError();
        }
        auto result = ir::parseTo(sources.front().text(), context, mod);
        if (!result) {
            err() << Error;
            ir::print(result.error(), err());
            return 1;
        }
        tryInvoke(callbacks.irgenCallback, context, mod);
        if (!continueCompilation) return 0;
        break;
    }
    }
    if (optLevel > 0) {
        opt::optimize(context, mod, optLevel);
    }
    else if (!optPipeline.empty()) {
        auto pipeline = ir::PassManager::makePipeline(optPipeline);
        pipeline(context, mod);
    }
    tryInvoke(callbacks.optCallback, context, mod);
    if (!continueCompilation) return 0;
    switch (targetType) {
    case TargetType::Executable:
        [[fallthrough]];
    case TargetType::BinaryOnly: {
        cg::NullLogger logger;
        if (!codegenLogger) {
            codegenLogger = &logger;
        }
        auto asmStream = cg::codegen(mod, *codegenLogger);
        tryInvoke(callbacks.codegenCallback, asmStream);
        if (!continueCompilation) return 0;
        auto asmRes = Asm::assemble(asmStream);
        tryInvoke(callbacks.asmCallback, asmRes);
        if (!continueCompilation) return 0;
        auto& [program, symbolTable, unresolved] = asmRes;
        auto linkRes =
            Asm::link(program, semaSym.foreignLibraryPaths(), unresolved);
        if (!linkRes) {
            printLinkerError(linkRes.error(), err());
            return handleError();
        }
        tryInvoke(callbacks.linkerCallback, program);
        if (!continueCompilation) return 0;
        std::string dsym = genDebugInfo ? Asm::generateDebugSymbols(asmStream) :
                                          std::string{};
        /// We emit the executable
        if (guardFileEmission("executable")) {
            writeExecutableFile(outputFile,
                                program,
                                { .executable = targetType ==
                                                TargetType::Executable });
        }
        if (genDebugInfo && guardFileEmission("debug info")) {
            auto file = createFile(appendExt(outputFile, "scdsym"));
            file << dsym;
        }
        break;
    }
    case TargetType::StaticLibrary: {
        if (guardFileEmission("static lib")) {
            auto symfile = createFile(appendExt(outputFile, "scsym"));
            sema::serialize(semaSym, symfile);
            auto irfile = createFile(appendExt(outputFile, "scir"));
            ir::print(mod, irfile);
        }
        break;
    }
    }
    return 0;
}

bool CompilerInvocation::guardFileEmission(std::string_view kind) {
    if (!outputFile.empty()) {
        return true;
    }
    err() << Warning << "Not writing " << kind
          << " to disk: No output specified\n";
    return false;
}

int CompilerInvocation::handleError() {
    if (errorHandler) {
        errorHandler();
    }
    return 1;
}
