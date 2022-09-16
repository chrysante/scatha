#include <Catch/Catch2.hpp>

#include "Common/Token.h"
#include "Parser/Keyword.h"

using namespace scatha;
using namespace parse;

TEST_CASE("Convert token to enum class keyword", "[parse]") {
	using enum Keyword;
	
	CHECK(toKeyword(Token{ "void" }).value() == Void);
	CHECK(toKeyword(Token{ "fn" }).value() == Function);
	CHECK(toKeyword(Token{ "while" }).value() == While);
	CHECK(toKeyword(Token{ "false" }).value() == False);
	
	CHECK(!toKeyword(Token{ "foo" }).has_value());
	CHECK(!toKeyword(Token{ "bar" }).has_value());
	
}

TEST_CASE("Keyword Categories", "[parse]") {
	
	using enum Keyword;
	
	CHECK(categorize(Void) == KeywordCategory::Types);
	CHECK(categorize(Bool) == KeywordCategory::Types);
	CHECK(categorize(Int) == KeywordCategory::Types);
	CHECK(categorize(Float) == KeywordCategory::Types);
	CHECK(categorize(String) == KeywordCategory::Types);
	
	CHECK(categorize(Import) == KeywordCategory::Modules);
	CHECK(categorize(Export) == KeywordCategory::Modules);
	
	CHECK(categorize(Module) == KeywordCategory::Declarators);
	CHECK(categorize(Class) == KeywordCategory::Declarators);
	CHECK(categorize(Struct) == KeywordCategory::Declarators);
	CHECK(categorize(Function) == KeywordCategory::Declarators);
	CHECK(categorize(Var) == KeywordCategory::Declarators);
	CHECK(categorize(Let) == KeywordCategory::Declarators);
	
	CHECK(categorize(Return) == KeywordCategory::ControlFlow);
	CHECK(categorize(If) == KeywordCategory::ControlFlow);
	CHECK(categorize(Else) == KeywordCategory::ControlFlow);
	CHECK(categorize(For) == KeywordCategory::ControlFlow);
	CHECK(categorize(While) == KeywordCategory::ControlFlow);
	CHECK(categorize(Do) == KeywordCategory::ControlFlow);
	
	CHECK(categorize(False) == KeywordCategory::BooleanLiterals);
	CHECK(categorize(True) == KeywordCategory::BooleanLiterals);
	
	CHECK(categorize(Public) == KeywordCategory::AccessSpecifiers);
	CHECK(categorize(Protected) == KeywordCategory::AccessSpecifiers);
	CHECK(categorize(Private) == KeywordCategory::AccessSpecifiers);
	
	CHECK(categorize(Placeholder) == KeywordCategory::Placeholder);
	
}
