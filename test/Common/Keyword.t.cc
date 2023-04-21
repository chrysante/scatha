#include <Catch/Catch2.hpp>

#include "AST/Keyword.h"
#include "AST/Token.h"

using namespace scatha;

TEST_CASE("Convert token to enum class keyword", "[parse]") {
    using enum Keyword;
    CHECK(toKeyword("void").value() == Void);
    CHECK(toKeyword("fn").value() == Function);
    CHECK(toKeyword("while").value() == While);
    CHECK(toKeyword("false").value() == False);
    CHECK(!toKeyword("foo").has_value());
    CHECK(!toKeyword("bar").has_value());
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
