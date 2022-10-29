#include "BasicCompiler.h"

#include <stdexcept>

#include "Assembly/Assembler.h"
#include "Basic/Memory.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/Canonicalize.h"
#include "IC/TacGenerator.h"
#include "Issue/IssueHandler.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

namespace scatha::test {

vm::Program compile(std::string_view text) {
    lex::Lexer l(text);
    auto tokens = l.lex();
    parse::Parser p(tokens);
    auto ast = p.parse();
    issue::IssueHandler iss;
    auto sym = sema::analyze(*ast, iss);
    if (!iss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    ic::canonicalize(ast.get());
    auto const tac = ic::generateTac(*ast, sym);
    codegen::CodeGenerator cg(tac);
    auto const str = cg.run();
    assembly::Assembler a(str);

    // start execution with main if it exists
    auto const mainID = [&sym] {
        auto const id  = sym.lookup("main");
        auto const* os = sym.tryGetOverloadSet(id);
        if (!os) {
            return sema::SymbolID::Invalid;
        }
        auto const* mainFn = os->find(std::array<sema::TypeID, 0>{});
        if (!mainFn) {
            return sema::SymbolID::Invalid;
        }
        return mainFn->symbolID();
    }();

    return a.assemble({ .mainID = mainID.rawValue() });
}

vm::VirtualMachine compileAndExecute(std::string_view text) {
    vm::Program const p = compile(text);
    vm::VirtualMachine vm;
    vm.load(p);
    vm.execute();
    return vm;
}

utl::vector<u64> getRegisters(std::string_view text) {
    auto vm = test::compileAndExecute(text);
    return vm.getState().registers;
}

} // namespace scatha::test
