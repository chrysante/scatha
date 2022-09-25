#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/Traversal.h"
#include "Assembly/Assembler.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/TacGenerator.h"
#include "IC/Canonicalize.h"
#include "IC/PrintTAC.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ExpressionParser.h"
#include "Sema/SemanticAnalyzer.h"
#include "Sema/PrintSymbolTable.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;


__attribute__((weak)) int main() {
	
	auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
	std::fstream file(filepath);
	if (!file) {
		std::cout << "Failed to open file " << filepath << std::endl;
	}
	
	std::stringstream sstr;
	sstr << file.rdbuf();
	std::string const text = sstr.str();
	
	try {
		Lexer l(text);
		auto tokens = l.lex();
		
		Parser p(tokens);
		auto ast = p.parse();
		
		sema::SemanticAnalyzer s;
		s.run(ast.get());
		
		ic::canonicalize(ast.get());
		
		ic::TacGenerator t(s.symbolTable());
		auto const tac = t.run(ast.get());
		
		ic::printTac(tac, s.symbolTable());
		
		codegen::CodeGenerator cg(tac);
		auto const str = cg.run();
		
		assembly::Assembler a(str);
		
		auto const program = a.assemble();
		print(program);
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	
}

