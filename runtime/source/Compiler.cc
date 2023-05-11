#include <scatha/Runtime/Compiler.h>

#include <iostream>

#include <svm/VirtualMachine.h>
#include <utl/vector.hpp>

#include "AST/LowerToIR.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "CommonImpl.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Issue/IssueHandler.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

struct Compiler::Impl {
    static constexpr size_t FunctionSlot = 2;

    sema::SymbolTable sym;
    size_t slotIndex = 0;
};

Compiler::Compiler(): impl(std::make_unique<Impl>()) {}

Compiler::Compiler(Compiler&&) noexcept = default;

Compiler& Compiler::operator=(Compiler&&) noexcept = default;

Compiler::~Compiler() = default;

std::optional<ExtFunctionID> Compiler::declareFunction(
    std::string name, QualType returnType, std::span<QualType const> argTypes) {
    ExtFunctionID const result{ impl->FunctionSlot, impl->slotIndex };
    bool success =
        impl->sym.declareSpecialFunction(sema::FunctionKind::External,
                                         std::move(name),
                                         result.slot,
                                         result.index,
                                         toSemaSig(impl->sym,
                                                   returnType,
                                                   argTypes),
                                         sema::FunctionAttribute::None);
    if (success) {
        ++impl->slotIndex;
        return result;
    }
    return std::nullopt;
}

std::unique_ptr<Program> Compiler::compile() {
    return compile(std::cout);
}

std::unique_ptr<Program> Compiler::compile(IssueHandler& iss) {
    if (sources.empty()) {
        return nullptr;
    }
    auto text = sources.front();

    auto astRoot = parse::parse(text, iss);
    if (!astRoot) {
        return nullptr;
    }

    sema::analyze(*astRoot, impl->sym, iss);
    if (iss.haveErrors()) {
        return nullptr;
    }

    ir::Context ctx;
    auto mod       = ast::lowerToIR(*astRoot, impl->sym, ctx);
    auto asmStream = cg::codegen(mod);
    auto asmResult = Asm::assemble(asmStream);

    return std::make_unique<Program>(std::move(asmResult.symbolTable),
                                     std::move(asmResult.program));
}

std::unique_ptr<Program> Compiler::compile(std::ostream& ostream) {
    IssueHandler iss;
    auto result = compile(iss);
    if (!iss.empty()) {
        iss.print(sources.front());
    }
    return result;
}
