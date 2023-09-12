#include <scatha/Runtime/Compiler.h>

#include <iostream>
#include <memory>

#include <svm/VirtualMachine.h>
#include <utl/vector.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "CommonImpl.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Opt/Optimizer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

struct Compiler::Impl {
    static constexpr size_t FunctionSlot = 2;

    Impl(): sym(std::make_unique<sema::SymbolTable>()) {}

    std::unique_ptr<sema::SymbolTable> sym;
    size_t slotIndex = 0;
};

Compiler::Compiler(): impl(std::make_unique<Impl>()) {}

Compiler::Compiler(Compiler&&) noexcept = default;

Compiler& Compiler::operator=(Compiler&&) noexcept = default;

Compiler::~Compiler() = default;

std::optional<ForeignFunctionID> Compiler::declareFunction(
    std::string name, QualType returnType, std::span<QualType const> argTypes) {
    ForeignFunctionID const result{ impl->FunctionSlot, impl->slotIndex };
    bool success =
        impl->sym->declareSpecialFunction(sema::FunctionKind::Foreign,
                                          std::move(name),
                                          result.slot,
                                          result.index,
                                          toSemaSig(*impl->sym,
                                                    returnType,
                                                    argTypes),
                                          sema::FunctionAttribute::None);
    if (success) {
        ++impl->slotIndex;
        return result;
    }
    return std::nullopt;
}

std::unique_ptr<Program> Compiler::compile(CompilationSettings settings) {
    return compile(settings, std::cout);
}

std::unique_ptr<Program> Compiler::compile(CompilationSettings settings,
                                           IssueHandler& iss) {
    if (sources.empty()) {
        return nullptr;
    }
    auto text = sources.front();
    auto astRoot = parse::parse(text, iss);
    if (!astRoot) {
        return nullptr;
    }
    auto analysisResult = sema::analyze(*astRoot, *impl->sym, iss);
    if (iss.haveErrors()) {
        return nullptr;
    }
    auto [ctx, mod] = irgen::generateIR(*astRoot, *impl->sym, analysisResult);
    if (settings.optimize) {
        opt::optimize(ctx, mod, 1);
    }
    auto asmStream = cg::codegen(mod);
    auto asmResult = Asm::assemble(asmStream);
    for (auto* f: impl->sym->functions()) {
        if (!f->isNative() ||
            f->accessSpecifier() != sema::AccessSpecifier::Public)
        {
            continue;
        }
        auto itr = asmResult.symbolTable.find(f->mangledName());
        SC_ASSERT(
            itr != asmResult.symbolTable.end(),
            "Public (externally visible) functions must have a binary address");
        f->setBinaryAddress(itr->second);
    }
    auto result = std::make_unique<Program>(std::move(asmResult.program),
                                            std::move(impl->sym));
    *this = Compiler();
    return result;
}

std::unique_ptr<Program> Compiler::compile(CompilationSettings settings,
                                           std::ostream& ostream) {
    IssueHandler iss;
    auto result = compile(settings, iss);
    if (!iss.empty()) {
        iss.print(sources.front());
    }
    return result;
}
