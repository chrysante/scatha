// clang-format off

// ===----------------------------------------------------------------------===
// === List of all keyword tokens ------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_KEYWORD_TOKEN_DEF
#   define SC_KEYWORD_TOKEN_DEF(Token, Spelling)
#endif

/// ## Types
SC_KEYWORD_TOKEN_DEF(Void,        "void")
SC_KEYWORD_TOKEN_DEF(Byte,        "byte")
SC_KEYWORD_TOKEN_DEF(Bool,        "bool")
SC_KEYWORD_TOKEN_DEF(Signed8,     "s8")
SC_KEYWORD_TOKEN_DEF(Signed16,    "s16")
SC_KEYWORD_TOKEN_DEF(Signed32,    "s32")
SC_KEYWORD_TOKEN_DEF(Signed64,    "s64")
SC_KEYWORD_TOKEN_DEF(Unsigned8,   "u8")
SC_KEYWORD_TOKEN_DEF(Unsigned16,  "u16")
SC_KEYWORD_TOKEN_DEF(Unsigned32,  "u32")
SC_KEYWORD_TOKEN_DEF(Unsigned64,  "u64")
SC_KEYWORD_TOKEN_DEF(Float32,     "f32")
SC_KEYWORD_TOKEN_DEF(Float64,     "f64")
SC_KEYWORD_TOKEN_DEF(Int,         "int")
SC_KEYWORD_TOKEN_DEF(Float,       "float")
SC_KEYWORD_TOKEN_DEF(Double,      "double")

/// ## Directives
SC_KEYWORD_TOKEN_DEF(Import,      "import")
SC_KEYWORD_TOKEN_DEF(Use,         "use")
SC_KEYWORD_TOKEN_DEF(Export,      "export")

/// ## Declarators
SC_KEYWORD_TOKEN_DEF(Module,      "module")
SC_KEYWORD_TOKEN_DEF(Struct,      "struct")
SC_KEYWORD_TOKEN_DEF(Protocol,    "protocol")
SC_KEYWORD_TOKEN_DEF(Function,    "fn")
SC_KEYWORD_TOKEN_DEF(Var,         "var")
SC_KEYWORD_TOKEN_DEF(Let,         "let")

/// ## Qualifiers
SC_KEYWORD_TOKEN_DEF(Mutable,     "mut")
SC_KEYWORD_TOKEN_DEF(Dynamic,     "dyn")

/// ## Control flow
SC_KEYWORD_TOKEN_DEF(Return,      "return")
SC_KEYWORD_TOKEN_DEF(If,          "if")
SC_KEYWORD_TOKEN_DEF(Else,        "else")
SC_KEYWORD_TOKEN_DEF(For,         "for")
SC_KEYWORD_TOKEN_DEF(While,       "while")
SC_KEYWORD_TOKEN_DEF(Do,          "do")
SC_KEYWORD_TOKEN_DEF(Break,       "break")
SC_KEYWORD_TOKEN_DEF(Continue,    "continue")

/// ## Operators
SC_KEYWORD_TOKEN_DEF(Unique,      "unique")
SC_KEYWORD_TOKEN_DEF(Reinterpret, "reinterpret")
SC_KEYWORD_TOKEN_DEF(Move,        "move")
SC_KEYWORD_TOKEN_DEF(As,          "as")

/// ## Constants
SC_KEYWORD_TOKEN_DEF(False,       "false")
SC_KEYWORD_TOKEN_DEF(True,        "true")
SC_KEYWORD_TOKEN_DEF(Null,        "null")
SC_KEYWORD_TOKEN_DEF(This,        "this")

/// ## Specifiers
SC_KEYWORD_TOKEN_DEF(Private,     "private")
SC_KEYWORD_TOKEN_DEF(Internal,    "internal")
SC_KEYWORD_TOKEN_DEF(Public,      "public")
SC_KEYWORD_TOKEN_DEF(Extern,      "extern")

#undef SC_KEYWORD_TOKEN_DEF

// ===----------------------------------------------------------------------===
// === List of all operator tokens -----------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_OPERATOR_TOKEN_DEF
#   define SC_OPERATOR_TOKEN_DEF(Token, Spelling)
#endif

SC_OPERATOR_TOKEN_DEF(Plus,              "+")
SC_OPERATOR_TOKEN_DEF(Minus,             "-")
SC_OPERATOR_TOKEN_DEF(Multiplies,        "*")
SC_OPERATOR_TOKEN_DEF(Divides,           "/")
SC_OPERATOR_TOKEN_DEF(Remainder,         "%")
SC_OPERATOR_TOKEN_DEF(BitAnd,            "&")
SC_OPERATOR_TOKEN_DEF(BitOr,             "|")
SC_OPERATOR_TOKEN_DEF(BitXOr,            "^")
SC_OPERATOR_TOKEN_DEF(Exclam,            "!")
SC_OPERATOR_TOKEN_DEF(Tilde,             "~")
SC_OPERATOR_TOKEN_DEF(Increment,         "++")
SC_OPERATOR_TOKEN_DEF(Decrement,         "--")
SC_OPERATOR_TOKEN_DEF(LeftShift,         "<<")
SC_OPERATOR_TOKEN_DEF(RightShift,        ">>")
SC_OPERATOR_TOKEN_DEF(LogicalAnd,        "&&")
SC_OPERATOR_TOKEN_DEF(LogicalOr,         "||")
SC_OPERATOR_TOKEN_DEF(Assign,            "=")
SC_OPERATOR_TOKEN_DEF(PlusAssign,        "+=")
SC_OPERATOR_TOKEN_DEF(MinusAssign,       "-=")
SC_OPERATOR_TOKEN_DEF(MultipliesAssign,  "*=")
SC_OPERATOR_TOKEN_DEF(DividesAssign,     "/=")
SC_OPERATOR_TOKEN_DEF(RemainderAssign,   "%=")
SC_OPERATOR_TOKEN_DEF(LeftShiftAssign,   "<<=")
SC_OPERATOR_TOKEN_DEF(RightShiftAssign,  ">>=")
SC_OPERATOR_TOKEN_DEF(AndAssign,         "&=")
SC_OPERATOR_TOKEN_DEF(OrAssign,          "|=")
SC_OPERATOR_TOKEN_DEF(XOrAssign,         "^=")
SC_OPERATOR_TOKEN_DEF(Equal,             "==")
SC_OPERATOR_TOKEN_DEF(Unequal,           "!=")
SC_OPERATOR_TOKEN_DEF(Less,              "<")
SC_OPERATOR_TOKEN_DEF(LessEqual,         "<=")
SC_OPERATOR_TOKEN_DEF(Greater,           ">")
SC_OPERATOR_TOKEN_DEF(GreaterEqual,      ">=")
SC_OPERATOR_TOKEN_DEF(Dot,               ".")
SC_OPERATOR_TOKEN_DEF(Arrow,             "->")
SC_OPERATOR_TOKEN_DEF(Question,          "?")

#undef SC_OPERATOR_TOKEN_DEF

// ===----------------------------------------------------------------------===
// === List of all punctuation tokens --------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_PUNCTUATION_TOKEN_DEF
#   define SC_PUNCTUATION_TOKEN_DEF(Token, str)
#endif

SC_PUNCTUATION_TOKEN_DEF(OpenParan,    "(")
SC_PUNCTUATION_TOKEN_DEF(CloseParan,   ")")
SC_PUNCTUATION_TOKEN_DEF(OpenBrace,    "{")
SC_PUNCTUATION_TOKEN_DEF(CloseBrace,   "}")
SC_PUNCTUATION_TOKEN_DEF(OpenBracket,  "[")
SC_PUNCTUATION_TOKEN_DEF(CloseBracket, "]")
SC_PUNCTUATION_TOKEN_DEF(Comma,        ",")
SC_PUNCTUATION_TOKEN_DEF(Semicolon,    ";")
SC_PUNCTUATION_TOKEN_DEF(Colon,        ":")

#undef SC_PUNCTUATION_TOKEN_DEF

// ===----------------------------------------------------------------------===
// === List of other tokens ------------------------------------------------===
// ===----------------------------------------------------------------------===

#ifndef SC_OTHER_TOKEN_DEF
#   define SC_OTHER_TOKEN_DEF(Token, str)
#endif

SC_OTHER_TOKEN_DEF(Identifier,             "")
SC_OTHER_TOKEN_DEF(IntegerLiteral,         "")
SC_OTHER_TOKEN_DEF(FloatLiteral,           "")
SC_OTHER_TOKEN_DEF(StringLiteral,          "")
SC_OTHER_TOKEN_DEF(FStringLiteralBegin,    "")
SC_OTHER_TOKEN_DEF(FStringLiteralContinue, "")
SC_OTHER_TOKEN_DEF(FStringLiteralEnd,      "")
SC_OTHER_TOKEN_DEF(CharLiteral,            "")
SC_OTHER_TOKEN_DEF(EndOfFile,              "")

#undef SC_OTHER_TOKEN_DEF
