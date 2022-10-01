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
#include "Sema/Analyze.h"
#include "Sema/PrintSymbolTable.h"

#include "Sema/Prepass.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;


[[gnu::weak]] int main() {
	auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
	std::fstream file(filepath);
	if (!file) {
		std::cout << "Failed to open file " << filepath << std::endl;
	}
	
	std::stringstream sstr;
	sstr << file.rdbuf();
	std::string const text = sstr.str();
	
	try {
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Regenerated Source Code ======================\n";
		std::cout <<   "==================================================\n\n";
		Lexer l(text);
		auto tokens = l.lex();
		Parser p(tokens);
		auto ast = p.parse();
		ast::printSource(ast.get());
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Symbol Tabele ================================\n";
		std::cout <<   "==================================================\n\n";
//		auto const sym = sema::analyze(ast.get());
		sema::SymbolTable sym;
		sema::prepass(*ast, sym);
		sema::printSymbolTable(sym);
		return 0;
		
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Generated Three Address Code =================\n";
		std::cout <<   "==================================================\n\n";
		ic::canonicalize(ast.get());
		ic::TacGenerator t(sym);
		auto const tac = t.run(ast.get());
		ic::printTac(tac, sym);
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Generated Assembly ===========================\n";
		std::cout <<   "==================================================\n\n";
		codegen::CodeGenerator cg(tac);
		auto const str = cg.run();
		print(str, sym);
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Assembled Program ============================\n";
		std::cout <<   "==================================================\n\n";
		assembly::Assembler a(str);
		auto const program = a.assemble();
		print(program);

		std::cout << "\n==================================================\n\n";
	}
	catch (std::exception const& e) {
		std::cout << e.what() << std::endl;
	}
}
