#include "Invocation/CompilerInvocation.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>

#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/format_time.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "Common/FileHandling.h"
#include "Common/SourceFile.h"
#include "Common/UniquePtr.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PassManager.h"
#include "IR/Print.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Opt/Passes.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/NameMangling.h"
#include "Sema/Serialize.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

[[maybe_unused]] static utl::vstreammanip<> const Info = [](std::ostream& str) {
    str << tfmt::format(tfmt::Green | tfmt::Bold, "Info: ");
};

[[maybe_unused]] static utl::vstreammanip<> const Warning =
    [](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
};

[[maybe_unused]] static utl::vstreammanip<> const Error =
    [](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
};

CompilerInvocation::CompilerInvocation(TargetType targetType, std::string name):
    targetType(targetType), name(name), errStream(&std::cout) {}

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

static void printLinkerError(Asm::LinkerError const& error, std::ostream& str) {
    str << Error << "Linker failed to resolve symbol references:\n";
    for (auto& symbol: error.missingSymbols) {
        str << "  - " << symbol << "\n";
    }
}

static void populateScopeWithBinaryInfo(sema::Scope& scope,
                                        Asm::AssemblerResult const& asmRes) {
    auto& asmSym = asmRes.symbolTable;
    for (auto* function: scope.entities() | Filter<sema::Function>) {
        if (!function->isPublic()) {
            continue;
        }
        auto mangler = sema::NameMangler();
        auto name = mangler(*function);
        auto itr = asmSym.find(name);
        if (itr != asmSym.end()) {
            function->setBinaryAddress(itr->second);
        }
    }
    for (auto* child: scope.children()) {
        if (!isa<sema::Function>(child)) {
            populateScopeWithBinaryInfo(*child, asmRes);
        }
    }
}

/// Recursively iterates all public scopes and sets the binary address of public
/// functions
static void populateSymbolTableWithBinaryInfo(
    sema::SymbolTable& sym, Asm::AssemblerResult const& asmRes) {
    populateScopeWithBinaryInfo(sym.globalScope(), asmRes);
}

std::optional<Target> CompilerInvocation::run() {
    using enum TargetType;
    /// Mode validation
    if (frontend == FrontendType::IR) {
        if (targetType != Executable && targetType != BinaryOnly) {
            err() << Error << "Can only generate executables with IR frontend"
                  << std::endl;
            handleError();
            return std::nullopt;
        }
    }
    if (sources.empty()) {
        err() << Error << "No input files" << std::endl;
        handleError();
        return std::nullopt;
    }
    /// Now we compile the program
    sema::SymbolTable semaSym;
    ir::Context irContext;
    ir::Module irModule;
    switch (frontend) {
    case FrontendType::Scatha: {
        IssueHandler issueHandler;
        auto ast = parser::parse(sources, issueHandler);
        if (!issueHandler.empty()) {
            issueHandler.print(sources);
        }
        if (!ast) {
            handleError();
            return std::nullopt;
        }
        auto analysisResult =
            sema::analyze(*ast, semaSym, issueHandler,
                          { .librarySearchPaths = libSearchPaths });
        if (!issueHandler.empty()) {
            issueHandler.print(sources, err());
        }
        tryInvoke(callbacks.frontendCallback, *ast, semaSym);
        if (!continueCompilation) return std::nullopt;
        if (issueHandler.haveErrors()) {
            handleError();
            return std::nullopt;
        }
        irgen::Config irgenConfig = { .sourceFiles = sources,
                                      .generateDebugSymbols = genDebugInfo };
        if (targetType == TargetType::StaticLibrary) {
            irgenConfig.nameMangler =
                sema::NameMangler({ .globalPrefix = name });
        }
        irgen::generateIR(irContext, irModule, *ast, semaSym, analysisResult,
                          std::move(irgenConfig));
        tryInvoke(callbacks.irgenCallback, irContext, irModule);
        semaSym.prepareExport();
        if (!continueCompilation) return std::nullopt;
        break;
    }
    case FrontendType::IR: {
        if (sources.size() != 1) {
            err() << Error << "Can only parse one IR file" << std::endl;
            handleError();
            return std::nullopt;
        }
        auto parseIssues =
            ir::parseTo(sources.front().text(), irContext, irModule);
        if (!parseIssues.empty()) {
            err() << Error;
            for (auto& issue: parseIssues) {
                ir::print(issue, err());
            }
            return std::nullopt;
        }
        tryInvoke(callbacks.irgenCallback, irContext, irModule);
        if (!continueCompilation) return std::nullopt;
        break;
    }
    }
    if (optLevel > 0) {
        opt::optimize(irContext, irModule);
    }
    else if (!optPipeline.empty()) {
        auto pipeline = ir::PassManager::makePipeline(optPipeline);
        pipeline(irContext, irModule);
    }
    tryInvoke(callbacks.optCallback, irContext, irModule);
    if (!continueCompilation) return std::nullopt;
    switch (targetType) {
    case TargetType::Executable:
        [[fallthrough]];
    case TargetType::BinaryOnly: {
        cg::NullLogger logger;
        if (!codegenLogger) {
            codegenLogger = &logger;
        }
        auto asmStream = cg::codegen(irModule, *codegenLogger);
        tryInvoke(callbacks.codegenCallback, asmStream);
        if (!continueCompilation) return std::nullopt;
        auto asmRes = Asm::assemble(asmStream);
        tryInvoke(callbacks.asmCallback, asmRes);
        if (!continueCompilation) return std::nullopt;
        auto& [program, symbolTable, unresolved] = asmRes;
        auto linkRes = Asm::link(linkerOptions, program,
                                 semaSym.foreignLibraries(), unresolved);
        if (!linkRes) {
            printLinkerError(linkRes.error(), err());
            handleError();
            return std::nullopt;
        }
        tryInvoke(callbacks.linkerCallback, program);
        if (!continueCompilation) return std::nullopt;
        std::string dsym = genDebugInfo ? Asm::generateDebugSymbols(asmStream) :
                                          std::string{};
        populateSymbolTableWithBinaryInfo(semaSym, asmRes);
        return Target(targetType, name,
                      std::make_unique<sema::SymbolTable>(std::move(semaSym)),
                      std::move(program), std::move(dsym));
    }
    case TargetType::StaticLibrary: {
        std::stringstream symstr;
        sema::serialize(semaSym, symstr);
        opt::globalDCE(irContext, irModule);
        std::stringstream objstr;
        ir::print(irModule, objstr);
        return Target(TargetType::StaticLibrary, name,
                      std::make_unique<sema::SymbolTable>(std::move(semaSym)),
                      Target::StaticLib{ std::move(symstr).str(),
                                         std::move(objstr).str() });
    }
    default:
        SC_UNREACHABLE();
    }
}

void CompilerInvocation::handleError() {
    if (errorHandler) {
        errorHandler();
    }
}
