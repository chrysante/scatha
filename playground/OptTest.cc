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
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"

using namespace playground;
using namespace scatha;

static std::string readFileToString(std::filesystem::path filepath) {
    std::fstream file(filepath);
    if (!file) {
        std::cerr << "Failed to open file " << filepath << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    return std::move(sstr).str();
}

static auto compile(std::string_view text, sema::SymbolTable& sym) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (!lexIss.empty()) {
        std::cout << "Lexical issue on line "
                  << lexIss.issues()[0].sourceLocation().line << std::endl;
        throw;
    }
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        std::cout << "Syntax issue on line "
                  << parseIss.issues()[0].sourceLocation().line << std::endl;
        throw;
    }
    issue::SemaIssueHandler semaIss;
    sema::analyze(*ast, sym, semaIss);
    if (!semaIss.empty()) {
        std::cout << "Semantic issue on line "
                  << semaIss.issues()[0].sourceLocation().line << std::endl;
        throw;
    }
    ir::Context ctx;
    auto mod      = ast::lowerToIR(*ast, sym, ctx);
    auto assembly = cg::codegen(mod);
    return Asm::assemble(assembly);
}

void playground::optTest(std::filesystem::path path) {
    auto text = readFileToString(path);
    sema::SymbolTable semaSym;
    auto cppCallbackSig = sema::FunctionSignature({}, semaSym.Void());
    semaSym.declareExternalFunction("cppCallback",
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
        std::cout << "Back in C++ land\n";
        ctx->vm.execute(ctx->callback);
    };

    ctx.vm.setFunctionTableSlot(1, { { cppCallback, &ctx } });
    ctx.vm.loadProgram(prog.data());
    size_t main = findFn("main");
    ctx.vm.execute(main);
}
