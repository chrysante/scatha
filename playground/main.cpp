#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyUtil.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/TacGenerator.h"
#include "IC/Canonicalize.h"
#include "IC/PrintTac.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ExpressionParser.h"
#include "Sema/SemanticAnalyzer.h"
#include "Sema/PrintSymbolTable.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;

#ifdef __GNUC__
__attribute__((weak))
#endif
int main() {
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
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Regenerated Source Code ======================\n";
		std::cout <<   "==================================================\n\n";
		ast::printSource(ast.get());
		
		sema::SemanticAnalyzer s;
		s.run(ast.get());
		ic::canonicalize(ast.get());
		ic::TacGenerator t(s.symbolTable());
		auto const tac = t.run(ast.get());
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Generated Three Address Code =================\n";
		std::cout <<   "==================================================\n\n";
		
		ic::printTac(tac, s.symbolTable());
		
		codegen::CodeGenerator cg(tac);
		auto const str = cg.run();
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Generated Assembly ===========================\n";
		std::cout <<   "==================================================\n\n";
		print(str, s.symbolTable());

		assembly::Assembler a(str);
	

		auto const program = a.assemble();
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Assembled Program ============================\n";
		std::cout <<   "==================================================\n\n";
		print(program);
		std::cout << "\n==================================================\n\n";
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
}
