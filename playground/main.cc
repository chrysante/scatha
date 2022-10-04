#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

#include <utl/typeinfo.hpp>

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
		std::cout <<   "=== AST ==========================================\n";
		std::cout <<   "==================================================\n\n";
		Lexer l(text);
		auto tokens = l.lex();
		Parser p(tokens);
		auto ast = p.parse();
		ast::printTree(ast.get());
		
		std::cout << "\n==================================================\n";
		std::cout <<   "=== Symbol Table =================================\n";
		std::cout <<   "==================================================\n\n";
		issue::IssueHandler iss;
		auto const sym = sema::analyze(ast.get(), iss);
//		auto const sym = sema::prepass(*ast, iss);
		sema::printSymbolTable(sym);
		std::cout << "\nEncoutered " << iss.semaIssues().size() << " issues\n";
		std::cout <<   "==================================================\n";
		for (auto const& issue: iss.semaIssues()) {
			issue.visit([](auto const& issue) {
				auto const loc = issue.token().sourceLocation;
				std::cout << "Line " << loc.line << " Col " << loc.column << ": ";
				std::cout << utl::nameof<std::decay_t<decltype(issue)>> << "\n\t";
			});
			issue.visit(utl::visitor{
				[&](sema::InvalidDeclaration const& e) {
					std::cout << "Invalid declaration (" << e.reason() << "): ";
					std::cout << std::endl;
				},
				[&](sema::BadTypeConversion const& e) {
					std::cout << "Bad type conversion: ";
					ast::printExpression(e.expression());
					std::cout << std::endl;
					std::cout << "\tFrom " << sym.getName(e.from()) << " to " << sym.getName(e.to()) << "\n";
				},
				[&](sema::BadFunctionCall const& e) {
					std::cout << "Bad function call: " << e.reason() << ": ";
					ast::printExpression(e.expression());
					std::cout << std::endl;
				},
				[&](sema::UseOfUndeclaredIdentifier const& e) {
					std::cout << "Use of undeclared identifier ";
					ast::printExpression(e.expression());
					std::cout << " in scope: " << e.currentScope().name() << std::endl;
				},
				[](issue::ProgramIssueBase const&) { std::cout << std::endl; }
			});
			std::cout << std::endl;
		}
		std::cout << "==================================================\n";
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
		std::cout << "Compilation failed:\n" << e.what() << std::endl;
	}
}
