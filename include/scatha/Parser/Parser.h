// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_PARSER_H_
#define SCATHA_PARSER_PARSER_H_

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Parser/Token.h>

// clang-format off

///
/// ## Grammar
///
/// ```
/// <translation-unit>              ::= {<external-declaration>}*
/// <external-declaration>          ::= [<access-spec>] <function-definition>
///                                   | [<access-spec>] <struct-definition>
/// <function-definition>           ::= "fn" <identifier> "(" {<parameter-declaration>}* ")" ["->" <type-expression>] <compound-statement>
/// <parameter-declaration>         ::= <identifier> ":" <type-expression>
///                                   | "this"
///                                   | "& this"
///                                   | "& mut this"
/// <struct-definition>             ::= "struct" <identifier> <compound-statement>
/// <variable-declaration>          ::= ("var"|"let") <short-var-declaration>
/// <short-var-declaration>         ::= <identifier> [":" <type-expression>] ["=" <assignment-expression>] ";"
/// <statement>                     ::= <external-declaration>
///                                   | <variable-declaration>
///                                   | <control-flow-statement>
///                                   | <compound-statement>
///                                   | <expression-statement>
///                                   | ";"
/// <expression-statement>          ::= <commma-expression> ";"
/// <compound-statement>            ::= "{" {<statement>}* "}"
/// <control-flow-statement>        ::= <return statement>
///                                   | <if-statement>
///                                   | <while-statement>
///                                   | <do-while-statement>
///                                   | <for-statement>
/// <return-statement>              ::= "return" [<comma-expression>] ";"
/// <if-statement>                  ::= "if" <comma-expression> <compound-statement> ["else" (<if-statement> | <compound-statement>)]
/// <while-statement>               ::= "while" <comma-expression> <compound-statement>
/// <do-while-statement>            ::= "do" <compound-statement> "while" <comma-expression> ";"
/// <for-statement>                 ::= "for" <short-var-declaration> <comma-expression> ";" <comma-expression> <compound-statement>
/// <break-statement>               ::= "break;"
/// <continue-statement>            ::= "continue;"
///
/// <type-expression>               ::= <reference-expression>
///
/// <comma-expression>              ::= <assignment-expression>
///                                   | <comma-expression> "," <assignment-expression>
/// <assignment-expression>         ::= <conditional-expression>
///                                   | <conditional-expression> "=, *=, ..." <assignment-expression>
/// <conditional-expression>        ::= <logical-or-expression>
///                                   | <logical-or-expression> "?" <comma-expression> ":" <conditional-expression>
/// <logical-or-expression>         ::= <logical-and-expression>
///                                   | <logical-or-expression> "||" <logical-and-expression>
/// <logical-and-expression>        ::= <inclusive-or-expression>
///                                   | <logical-and-expression> "&&" <inclusive-or-expression>
/// <inclusive-or-expression>       ::= <exclusive-or-expression>
///                                   | <inclusive-or-expression> "|" <exclusive-or-expression>
/// <exclusive-or-expression>       ::= <and-expression>
///                                   | <exclusive-or-expression> "^" <and-expression>
/// <and-expression>                ::= <equality-expression>
///                                   | <and-expression> "&" <equality-expression>
/// <equality-expression>           ::= <relational-expression>
///                                   | <equality-expression> "==" <relational-expression>
///                                   | <equality-expression> "!=" <relational-expression>
/// <relational-expression>         ::= <shift-expression>
///                                   | <relational-expression> "<"  <shift-expression>
///                                   | <relational-expression> ">" <shift-expression>
///                                   | <relational-expression> "<=" <shift-expression>
///                                   | <relational-expression> ">=" <shift-expression>
/// <shift-expression>              ::= <additive-expression>
///                                   | <shift-expression> "<<" <additive-expression>
///                                   | <shift-expression> ">>" <additive-expression>
/// <additive-expression>           ::= <multiplicative-expression>
///                                   | <additive-expression> "+" <multiplicative-expression>
///                                   | <additive-expression> "-" <multiplicative-expression>
/// <multiplicative-expression>     ::= <unary-expression>
///                                   | <multiplicative-expression> "*" <unary-expression>
///                                   | <multiplicative-expression> "/" <unary-expression>
///                                   | <multiplicative-expression> "%" <unary-expression>
/// <unary-expression>              ::= <reference-expression>
///                                   | "+" <unary-expression>
///                                   | "-" <unary-expression>
///                                   | "~" <unary-expression>
///                                   | "!" <unary-expression>
///                                   | "++" <unary-expression>
///                                   | "--" <unary-expression>
/// <reference-expression>          ::= <unique-expression> |
///                                     "&" ["unique"] ["mut"] <reference-expression>
/// <unique-expression>             ::= <postfix-expression>
///                                   | "unique" ["mut"] <function-call> /* Exposition only */
/// <postfix-expression>            ::= <generic-expression>
///                                   | <postfix-expression> "[" {<assignment-expression>}* "]"
///                                   | <postfix-expression> "(" {<assignment-expression>}* ")"
///                                   | <postfix-expression> "." <identifier>
/// <generic-expression>            ::= <primary-expression>
///                                   | <generic-id> "<" {<additive-expression>}* ">"
/// <primary-expression>            ::= <identifier>
///                                   | <integer-literal>
///                                   | <boolean-literal>
///                                   | <floating-point-literal>
///                                   | "this"
///                                   | <string-literal>
///                                   | "(" <comma-expression> ")"
///                                   | "[" {<conditional-expression>}* "]"
/// <access-spec>                   ::= "public" | "private"
/// ```
///
/// ## Operator precedence
///
/// ```
/// ┌────────────┬──────────────┬──────────────────────────────┬──────────────────┐
/// │ Precedence │ Operator     │ Description                  │ Associativity    │
/// ├────────────┼──────────────┼──────────────────────────────┼──────────────────┤
/// │  1         │ ()           │ Function call                │ Left to right -> │
/// │            │ []           │ Subscript                    │                  │
/// │            │ .            │ Member access                │                  │
/// ├────────────┼──────────────┼──────────────────────────────┼──────────────────┤
/// │  2         │ +, -         │ Unary plus and minus         │ Right to left <- │
/// │            │ !, ~         │ Logical and bitwise NOT      │                  │
/// │            │ &            │ Reference                    │                  │
/// │            │ unique       │ Dynamic memory allocation    │                  │
/// ├────────────┼──────────────┼──────────────────────────────┼──────────────────┤
/// │  3         │ *, /, %      │ Multiplication, division     │ Left to right -> │
/// │            │              │ and remainder                │                  │
/// │  4         │ +, -         │ Addition and subtraction     │                  │
/// │  5         │ <<, >>       │ Bitwise left and right shift │                  │
/// │  6         │ <, <=, >, >= │ Relational operators         │                  │
/// │  7         │ ==, !=       │ Equality operators           │                  │
/// │  8         │ &            │ Bitwise AND                  │                  │
/// │  9         │ ^            │ Bitwise XOR                  │                  │
/// │ 10         │ |            │ Bitwise OR                   │                  │
/// │ 11         │ &&           │ Logical AND                  │                  │
/// │ 12         │ ||           │ Logical OR                   │                  │
/// ├────────────┼──────────────┼──────────────────────────────┼──────────────────┤
/// │ 13         │ ?:           │ Conditional                  │ Right to left <- │
/// │            │ =, +=, -=    │ Assignment                   │ Right to left <- │
/// │            │ *=, /=, %=   │                              │                  │
/// │            │ <<=, >>=,    │                              │                  │
/// │            │ &=, |=,      │                              │                  │
/// ├────────────┼──────────────┼──────────────────────────────┼──────────────────┤
/// │ 14         │ ,            │ Comma operator               │ Left to right -> │
/// └────────────┴──────────────┴──────────────────────────────┴──────────────────┘
/// ```

// clang-format on

namespace scatha::parse {

/// Parse source code \p source into an abstract syntax tree
/// Issues will be submitted to \p issueHandler
/// \Returns The parsed AST or `nullptr` if lexical issues were encountered
SCATHA_API UniquePtr<ast::AbstractSyntaxTree> parse(std::string_view source,
                                                    IssueHandler& issueHandler);

} // namespace scatha::parse

#endif // SCATHA_PARSER_PARSER_H_