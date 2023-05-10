#include <scatha/Runtime.h>

#include <iostream>

#include <svm/VirtualMachine.h>
#include <utl/vector.hpp>

#include "AST/LowerToIR.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Issue/IssueHandler.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

sema::FunctionSignature toSemaSig(RuntimeFunction::Signature const& sig) {
    auto* returnType = static_cast<sema::QualType const*>(sig.returnType);
    auto args = sig.argTypes | ranges::views::transform([](void const* type) {
                    return static_cast<sema::QualType const*>(type);
                }) |
                ranges::to<utl::small_vector<sema::QualType const*>>;
    return sema::FunctionSignature(std::move(args), returnType);
}

void Runtime::compile() {
    if (sources.empty()) {
        return;
    }
    auto text = sources.front();
    IssueHandler iss;
    auto astRoot = parse::parse(text, iss);
    if (!iss.empty()) {
        iss.print(text);
        iss.clear();
    }
    if (!astRoot) {
        return;
    }

    sema::SymbolTable sym;
    utl::vector<svm::ExternalFunction> extFunctions;
    for (auto&& [index, fn]: functions | ranges::views::enumerate) {
        sym.declareSpecialFunction(sema::FunctionKind::External,
                                   fn.name(),
                                   2,
                                   index,
                                   toSemaSig(fn.buildSig(&sym)),
                                   sema::FunctionAttribute::None);
        extFunctions.push_back({ fn.pointer(), fn.userData() });
    }
    vm->setFunctionTableSlot(2, std::move(extFunctions));

    sema::analyze(*astRoot, sym, iss);
    if (!iss.empty()) {
        iss.print(text);
        return;
    }

    ir::Context ctx;
    auto mod = ast::lowerToIR(*astRoot, sym, ctx);

    auto asmStream = cg::codegen(mod);
    auto asmResult = Asm::assemble(asmStream);
    binary         = std::move(asmResult.program);
    symbolTable    = std::move(asmResult.symbolTable);

    vm->loadBinary(binary.data());
}

void Runtime::runImpl(std::string_view function,
                      std::span<uint64_t const> args,
                      std::span<uint8_t> retVal) {
    for (auto&& [name, address]: symbolTable) {
        if (!name.starts_with(function)) {
            continue;
        }
        auto* regPtr = vm->execute(address, args);
        if (retVal.empty()) {
            return;
        }
        std::memcpy(retVal.data(), regPtr, retVal.size());
        return;
    }
    std::cout << "Function '" << function << "' not found\n";
}
