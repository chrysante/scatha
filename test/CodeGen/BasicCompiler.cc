#include "BasicCompiler.h"

#include <stdexcept>

#include <Catch/Catch2.hpp>
#include <utl/format.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Basic/Memory.h"
#include "CodeGen/AST2IR/CodeGenerator.h"
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Opt/Mem2Reg.h"
#include "Issue/IssueHandler.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

static vm::Program compile(std::string_view text, bool optimze) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (!lexIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    issue::SemaIssueHandler semaIss;
    auto sym = sema::analyze(*ast, semaIss);
    if (!semaIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    ir::Context ctx;
    auto mod       = ast::codegen(*ast, sym, ctx);
    if (optimze) {
        opt::mem2Reg(ctx, mod);
    }
    auto asmStream = cg::codegen(mod);
    /// Start execution with main if it exists.
    auto const mainID = [&sym] {
        auto const id  = sym.lookup("main");
        auto const* os = sym.tryGetOverloadSet(id);
        if (!os) {
            return sema::SymbolID::Invalid;
        }
        auto const* mainFn = os->find({});
        if (!mainFn) {
            return sema::SymbolID::Invalid;
        }
        return mainFn->symbolID();
    }();
    return Asm::assemble(asmStream, { .startFunction = utl::format("main{:x}", mainID.rawValue()) });
}

static u64 compileAndExecute(std::string_view text, bool optimize) {
    vm::Program const p = compile(text, optimize);
    vm::VirtualMachine vm;
    vm.load(p);
    vm.execute();
    return vm.getState().registers[0];
}

void test::checkReturns(u64 value, std::string_view text) {
    bool const optimize = GENERATE(false, true);
    CHECK(compileAndExecute(text, optimize) == value);
}
