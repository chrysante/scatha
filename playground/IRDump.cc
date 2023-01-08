#include "IRDump.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <utl/stdio.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "CodeGen/AST2IR/CodeGenerator.h"
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"
#include "VM/Program.h"

using namespace scatha;

static void sectionHeader(std::string_view header) {
    utl::print("{:=^40}\n", "");
    utl::print("{:=^40}\n", header);
    utl::print("{:=^40}\n", "");
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

void playground::irDump(std::filesystem::path filepath) {
    irDump(readFileToString(filepath));
}

void playground::irDump(std::string text) {
    ir::Module mod = makeIRModule(text);
    sectionHeader(" IR Code ");
    ir::print(mod);
    
    auto asmStream = cg::codegen(mod);
    sectionHeader(" Assembly ");
    Asm::print(asmStream);

    auto program = Asm::assemble(asmStream);
    sectionHeader(" Assembled program ");
    print(program);
}

scatha::ir::Module playground::makeIRModule(std::string text) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (!lexIss.empty()) {
        std::cout << "Lexical issue on line " << lexIss.issues()[0].sourceLocation().line << std::endl;
        std::exit(EXIT_FAILURE);
    }
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        std::cout << "Syntax issue on line " << parseIss.issues()[0].sourceLocation().line << std::endl;
        std::exit(EXIT_FAILURE);
    }
    issue::SemaIssueHandler semaIss;
    auto sym = sema::analyze(*ast, semaIss);
    if (!semaIss.empty()) {
        std::cout << "Semantic issue on line " << semaIss.issues()[0].sourceLocation().line << std::endl;
        std::exit(EXIT_FAILURE);
    }
    ir::Context ctx;
    return ast::codegen(*ast, sym, ctx);
}

scatha::ir::Module playground::makeIRModule(std::filesystem::path filepath) {
    return makeIRModule(readFileToString(filepath));
}
