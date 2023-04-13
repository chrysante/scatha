#include "IRDump.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include <svm/Program.h>

#include "AST/LowerToIR.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "IR/Print.h"
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;

static void sectionHeader(std::string_view header) {
    std::cout << "===================="
              << "====================\n";
    std::cout << "====================" << header << "====================\n";
    std::cout << "===================="
              << "====================\n";
}

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

void playground::irDumpFromFile(std::filesystem::path filepath) {
    irDump(readFileToString(filepath));
}

void playground::irDump(std::string_view text) {
    auto [ctx, mod] = makeIRModule(text);
    sectionHeader(" IR Code ");
    ir::print(mod);

    auto asmStream = cg::codegen_Old(mod);
    sectionHeader(" Assembly ");
    Asm::print(asmStream);

    auto program = Asm::assemble(asmStream);
    sectionHeader(" Assembled program ");
    svm::print(program.data());
}

static std::optional<std::pair<scatha::ir::Context, scatha::ir::Module>>
    makeIRModuleFromSC(std::string_view text, std::ostream& errStr) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (!lexIss.empty()) {
        errStr << "Lexical issue on line "
               << lexIss.issues()[0].sourceLocation().line << std::endl;
        return std::nullopt;
    }
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        errStr << "Syntax issue on line "
               << parseIss.issues()[0].sourceLocation().line << std::endl;
        return std::nullopt;
    }
    issue::SemaIssueHandler semaIss;
    auto sym = sema::analyze(*ast, semaIss);
    if (!semaIss.empty()) {
        errStr << "Semantic issue on line "
               << semaIss.issues()[0].sourceLocation().line << std::endl;
        return std::nullopt;
    }
    ir::Context ctx;
    return std::pair{ std::move(ctx), ast::codegen(*ast, sym, ctx) };
}

static std::optional<std::pair<scatha::ir::Context, scatha::ir::Module>>
    makeIRModuleFromIR(std::string_view text, std::ostream& errStr) {
    auto res = ir::parse(text);
    if (!res) {
        ir::print(res.error(), errStr);
        return std::nullopt;
    }
    return std::move(res).value();
}

std::pair<scatha::ir::Context, scatha::ir::Module> playground::makeIRModule(
    std::string_view text) {
    std::stringstream scErr;
    auto res = makeIRModuleFromSC(text, scErr);
    if (res) {
        return *std::move(res);
    }
    std::stringstream irErr;
    res = makeIRModuleFromIR(text, irErr);
    if (res) {
        return *std::move(res);
    }
    std::cout << "Failed to parse text:\n";
    std::cout << "   Interpreted as .sc: " << scErr.str() << std::endl;
    std::cout << "   Interpreted as .scir: " << irErr.str() << std::endl;
    std::exit(EXIT_FAILURE);
}

std::pair<scatha::ir::Context, scatha::ir::Module> playground::
    makeIRModuleFromFile(std::filesystem::path filepath) {
    return makeIRModule(readFileToString(filepath));
}
