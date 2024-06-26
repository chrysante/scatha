#ifndef SCATHA_IR_PARSER_TOKEN_H_
#define SCATHA_IR_PARSER_TOKEN_H_

#include <optional>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/IR/Parser/IRSourceLocation.h>

namespace scatha::ir {

enum class TokenKind {
    Structure,
    Function,
    Global,
    Constant,

    OpenParan,
    CloseParan,
    OpenBrace,
    CloseBrace,
    OpenBracket,
    CloseBracket,
    Assign,
    Comma,
    Colon,

    IntLiteral,
    FloatLiteral,
    NullptrLiteral,
    UndefLiteral,
    StringLiteral,

    GlobalIdentifier,   // @...
    LocalIdentifier,    // %...
    MetadataIdentifier, // #...

    FIRST_BUILTIN_ID,
    Void = FIRST_BUILTIN_ID,
    Ptr,
    IntType,
    FloatType,

    Alloca,
    Load,
    Store,

#define SC_CONVERSION_DEF(Op, Keyword) Op,
#include <scatha/IR/Lists.def.h>

    Goto,
    Branch,
    Return,
    Call,
    Phi,
    SCmp,
    UCmp,
    FCmp,
    Bnt,
    Lnt,
    Neg,
    Add,
    Sub,
    Mul,
    SDiv,
    UDiv,
    SRem,
    URem,
    FAdd,
    FSub,
    FMul,
    FDiv,
    LShL,
    LShR,
    AShL,
    AShR,
    And,
    Or,
    XOr,
    GetElementPointer,
    InsertValue,
    ExtractValue,
    Select,

    Ext,
    To,
    Label,
    Inbounds,
    Equal,
    NotEqual,
    Less,
    LessEq,
    Greater,
    GreaterEq,

    ByVal,
    ValRet,

    OtherID, // All IDs that are not keywords and are not prefixed with @, % or
             // #

    LAST_BUILTIN_ID = OtherID,

    EndOfFile
};

/// \Returns `true` if \p kind is a builtin keyword or `OtherID`
inline bool isBuiltinID(TokenKind kind) {
    return (int)kind >= (int)TokenKind::FIRST_BUILTIN_ID &&
           (int)kind <= (int)TokenKind::LAST_BUILTIN_ID;
}

class Token {
public:
    explicit Token(std::string_view id, SourceLocation loc, TokenKind kind,
                   unsigned width = 0):
        _id(id), _loc(loc), _kind(kind), _width(width) {}

    explicit Token(char const* first, char const* last, SourceLocation loc,
                   TokenKind kind, unsigned width = 0):
        Token(std::string_view(first, static_cast<size_t>(last - first)), loc,
              kind, width) {}

    std::string_view id() const { return _id; }

    SourceLocation sourceLocation() const { return _loc; }

    TokenKind kind() const { return _kind; }

    /// Width of integral or float type. Only applicable if `kind() == IntType`
    /// or `kind() == FloatType`
    unsigned width() const { return _width; }

private:
    std::string_view _id;
    SourceLocation _loc;
    TokenKind _kind;
    union {
        unsigned _width;
    };
};

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_TOKEN_H_
