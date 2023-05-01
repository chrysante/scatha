#include "OptTest.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

#include "AST/LowerToIR.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "CodeGen/CodeGen.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "IR/Print.h"
#include "IRDump.h"
#include "Parser/Lexer.h"
#include "Parser/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Entity.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"

using namespace playground;
using namespace scatha;

static auto compile(std::string_view text, sema::SymbolTable& sym) {
    IssueHandler issues;
    auto tokens = parse::lex(text, issues);
    if (!issues.empty()) {
        issues.print(text);
        throw;
    }
    auto ast = parse::parse(tokens, issues);
    if (!issues.empty()) {
        issues.print(text);
        throw;
    }
    sema::analyze(*ast, sym, issues);
    if (!issues.empty()) {
        issues.print(text);
        throw;
    }
    ir::Context ctx;
    auto mod      = ast::lowerToIR(*ast, sym, ctx);
    auto assembly = cg::codegen(mod);
    return Asm::assemble(assembly);
}

static auto const text = R"(
public fn main() {
    var i = 1;
    print(i);
    i = cppCallback(i);
    print(i);
}

fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar(10);
}

fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

public fn callback(n: int) {
    print(n);
    print(fac(n));
}
)";

void playground::optTest(std::filesystem::path path) {
    sema::SymbolTable semaSym;
    auto cppCallbackSig =
        sema::FunctionSignature({ semaSym.qS64() }, semaSym.qS64());
    semaSym.declareSpecialFunction(sema::FunctionKind::External,
                                   "cppCallback",
                                   1,
                                   0,
                                   cppCallbackSig,
                                   sema::FunctionAttribute::None);
    auto [prog, sym] = compile(text, semaSym);

    auto findFn = [sym = &sym](std::string_view name) {
        auto itr = std::find_if(sym->begin(), sym->end(), [&](auto& p) {
            return p.first.starts_with(name);
        });
        assert(itr != sym->end());
        return itr->second;
    };

    struct Ctx {
        svm::VirtualMachine vm;
        size_t callback;
    } ctx{ .callback = findFn("callback") };

    auto cppCallback = [](u64* regPtr, svm::VirtualMachine*, void* c) {
        auto* ctx = reinterpret_cast<Ctx*>(c);
        std::cout << "C++ callback\n";
        uint64_t arg = regPtr[0];
        std::cout << "Invoking scatha callback\n";
        ctx->vm.execute(ctx->callback, std::array<uint64_t, 1>{ 6 });
        std::cout << "Leaving C++ callback\n";
        regPtr[0] = arg + 1;
    };

    ctx.vm.setFunctionTableSlot(1, { { cppCallback, &ctx } });
    ctx.vm.loadBinary(prog.data());
    size_t main = findFn("main");
    ctx.vm.execute(main, {});
}
