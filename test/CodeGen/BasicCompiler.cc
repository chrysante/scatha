#include "BasicCompiler.h"

#include <stdexcept>

#include <Catch/Catch2.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/format.hpp>
#include <utl/functional.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Basic/Memory.h"
#include "CodeGen/AST2IR/CodeGenerator.h"
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Issue/IssueHandler.h"
#include "Lexer/Lexer.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/Mem2Reg.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"

using namespace scatha;

struct OptimizationLevel {
    OptimizationLevel(std::invocable<ir::Context&, ir::Module&> auto&& f): optFunc(f) {}
    
    void run(ir::Context& ctx, ir::Module& mod) const {
        optFunc(ctx, mod);
    }
    
    utl::function<void(ir::Context&, ir::Module&)> optFunc;
};

static auto compile(std::string_view text, OptimizationLevel optLevel) {
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
    auto mod = ast::codegen(*ast, sym, ctx);
    optLevel.run(ctx, mod);
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

static u64 compileAndExecute(std::string_view text, OptimizationLevel optLevel) {
    auto const p = compile(text, optLevel);
    svm::VirtualMachine vm;
    vm.loadProgram(p.data());
    vm.execute();
    return vm.getState().registers[0];
}

void test::checkReturns(u64 value, std::string_view text) {
    // clang-format off
    utl::vector<OptimizationLevel> const levels = {
        [](ir::Context&, ir::Module&) {},
        [](ir::Context& ctx, ir::Module& mod) {
            for (auto& function: mod.functions()) {
                opt::mem2Reg(ctx, function);
            }
        },
        [](ir::Context& ctx, ir::Module& mod) {
            for (auto& function: mod.functions()) {
                opt::mem2Reg(ctx, function);
                opt::propagateConstants(ctx, function);
            }
        },
        [](ir::Context& ctx, ir::Module& mod) {
            for (auto& function: mod.functions()) {
                opt::mem2Reg(ctx, function);
                opt::propagateConstants(ctx, function);
                opt::dce(ctx, function);
            }
        },
    }; // clang-format on
    for (auto [index, level]: utl::enumerate(levels)) {
        CHECK(compileAndExecute(text, level) == value);
    }
}
