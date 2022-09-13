#include <iostream>
#include <string>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace scatha::parse;

int main() {
	
	std::string const text = R"(

fn mul(a: int, b: int) -> int {
	var result = -(f(1, 2 + 3))
}

)";
	
//	var result = a * b  + -c
//	var result = a + b % c
//	var result = f(1, 2 + 3)
//	var result = f(1, 2 + 3)
//	let x = a * (b + c)
//	let x: int = -y
//	let x
//
//	return result

//	CHECK_NOTHROW([&]{
	lex::Lexer l(text);
	auto tokens = l.lex();
	
	Allocator alloc;
	Parser p(tokens, alloc);
	auto const* const ast = p.parse();

	std::cout << *ast << std::endl;
}
