// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_KEYWORD_H_
#define SCATHA_AST_KEYWORD_H_

#include <optional>
#include <string_view>

#include <scatha/Common/Base.h>

namespace scatha {

enum class Keyword : u8 {
    Void,
    Bool,
    Int,
    Float,
    String,

    Import,
    Export,

    Module,
    Class,
    Struct,
    Function,
    Var,
    Let,

    Return,
    If,
    Else,
    For,
    While,
    Do,
    Break,
    Continue,

    False,
    True,

    Public,
    Protected,
    Private,

    Placeholder,

    _count
};

enum class KeywordCategory : u8 {
    Types,
    Modules,
    Declarators,
    ControlFlow,
    BooleanLiterals,
    AccessSpecifiers,
    Placeholder
};

SCATHA_API std::optional<Keyword> toKeyword(std::string_view);

SCATHA_API bool isDeclarator(Keyword);

SCATHA_API bool isControlFlow(Keyword);

SCATHA_API KeywordCategory categorize(Keyword);

} // namespace scatha

#endif // SCATHA_AST_COMMON_H_
