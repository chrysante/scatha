#include "Common/Keyword.h"

#include <string>

#include <utl/common.hpp>
#include <utl/vector.hpp>

namespace scatha {

static utl::vector<std::string> const keywords = [] {
    utl::vector<std::string> result{ size_t(Keyword::_count) };
    auto at = [&](Keyword k) -> auto& {
        return result[size_t(k)];
    };
    using enum Keyword;

    // MARK: Types
    // Fundamental types:
    at(Void)   = "void";
    at(Bool)   = "bool";
    at(Int)    = "int";
    at(Float)  = "float";
    at(String) = "string";

    // MARK: Modules
    // Import a module:
    // import foo
    // import bar
    //
    at(Import) = "import";
    // Export an identifier:
    // export fn foo()
    // export var bar: int = 0
    //
    at(Export) = "export";

    // MARK: Declarators
    // Declare a module:
    // module foo
    // module bar
    //
    at(Module) = "module";
    // Declare a class:
    // class foo
    // class bar
    //
    at(Class) = "class";
    // Declare a struct:
    // struct foo
    // struct bar
    //
    at(Struct) = "struct";
    // Declare a free function or member function:
    // fn foo(baz: int) -> int
    // fn bar(_ baz: int) -> void
    //
    at(Function) = "fn";
    // Declare a variable:
    // var foo = 0
    // var bar: int = 0
    // var baz = "Hello world"
    //
    at(Var) = "var";
    // Declare a constant:
    // let foo: int = 0
    //
    at(Let) = "let";

    // MARK: Control flow
    // Return:
    // return <expr>
    // return
    at(Return) = "return";
    // If/Else:
    // if (<condition>) <statement>
    // if (<condition>) <statement> else <statement>
    at(If)   = "if";
    at(Else) = "else";

    at(For) = "for";
    // While:
    // while (<cond>) <statement>
    at(While) = "while";
    // Do/While:
    // do <statement> while (<cond>)
    at(Do) = "do";

    // MARK: Boolean literals
    at(False) = "false";
    at(True)  = "true";

    // MARK: Access specifiers
    at(Public)    = "public";
    at(Protected) = "protected";
    at(Private)   = "private";

    // MARK: Placeholder
    at(Placeholder) = "_";

    return result;
}();

std::optional<Keyword> toKeyword(std::string_view id) {
    for (size_t i = 0; i < keywords.size(); ++i) {
        if (id == keywords[i]) {
            return Keyword(i);
        }
    }
    return std::nullopt;
}

bool isDeclarator(Keyword k) {
    switch (k) {
    case Keyword::Module:
    case Keyword::Class:
    case Keyword::Struct:
    case Keyword::Function:
    case Keyword::Var:
    case Keyword::Let: return true;
    default: return false;
    }
}

bool isControlFlow(Keyword k) {
    switch (k) {
    case Keyword::Return:
    case Keyword::If:
    case Keyword::Else:
    case Keyword::For:
    case Keyword::While:
    case Keyword::Do: return true;
    default: return false;
    }
}

KeywordCategory categorize(Keyword _k) {
    using enum Keyword;

    auto const k = utl::to_underlying(_k);

    if (k <= utl::to_underlying(String)) {
        return KeywordCategory::Types;
    }
    if (k <= utl::to_underlying(Export)) {
        return KeywordCategory::Modules;
    }
    if (k <= utl::to_underlying(Let)) {
        return KeywordCategory::Declarators;
    }
    if (k <= utl::to_underlying(Do)) {
        return KeywordCategory::ControlFlow;
    }
    if (k <= utl::to_underlying(True)) {
        return KeywordCategory::BooleanLiterals;
    }
    if (k <= utl::to_underlying(Private)) {
        return KeywordCategory::AccessSpecifiers;
    }
    if (k <= utl::to_underlying(Placeholder)) {
        return KeywordCategory::Placeholder;
    }

    SC_DEBUGFAIL();
}

} // namespace scatha
