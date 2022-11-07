#pragma once

#ifndef SCATHA_PARSER_PARSER2_H_
#define SCATHA_PARSER_PARSER2_H_

#include <utl/vector.hpp>

#include "AST/Base.h"
#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"


/**
 clang-format off
 
 MARK: Grammar
 \code
 
 <translation-unit>              ::= {<external-declaration>}*
 <external-declaration>          ::= <function-definition>
                                   | <struct-definition>
 <function-definition>           ::= "fn" <identifier> "(" {<parameter-declaration>}* ")" ["->" <type-expression>] <compound-statement>
 <parameter-declaration>         ::= <identifier> ":" <type-expression>
 <struct-definition>             ::= "struct" <identifier> <compound-statement>
 <variable-declaration>          ::= ("var"|"let") <identifier> [":" <type-expression>] ["=" <assignment-expression>] ";"
 <statement>                     ::= <external-declaration>
                                   | <variable-declaration>
                                   | <control-flow-statement>
                                   | <compound-statement>
                                   | <expression-statement>
                                   | ";"
 <expression-statement>          ::= <commma-expression> ";"
 <compound-statement>            ::= "{" {<statement>}* "}"
 <control-flow-statement>        ::= <return statement>
                                   | <if-statement>
                                   | <while-statement>
 <return-statement>              ::= "return" [<comma-expression>] ";"
 <if-statement>                  ::= "if" <comma-expression> <compound-statement> ["else" (<if-statement> | <compound-statement>)]
 <while-statement>               ::= "while" <comma-expression> <compound-statement>
 
 <comma-expression>              ::= <assignment-expression>
                                   | <comma-expression> "," <assignment-expression>
 <assignment-expression>         ::= <conditional-expression>
                                   | <conditional-expression> "=, *=, ..." <assignment-expression>
 <type-expression>               ::= <conditional-expression>
 <conditional-expression>        ::= <logical-or-expression>
                                   | <logical-or-expression> "?" <comma-expression> ":" <conditional-expression>
 <logical-or-expression>         ::= <logical-and-expression>
                                   | <logical-or-expression> "||" <logical-and-expression>
 <logical-and-expression>        ::= <inclusive-or-expression>
                                   | <logical-and-expression> "&&" <inclusive-or-expression>
 <inclusive-or-expression>       ::= <exclusive-or-expression>
                                   | <inclusive-or-expression> "|" <exclusive-or-expression>
 <exclusive-or-expression>       ::= <and-expression>
                                   | <exclusive-or-expression> "^" <and-expression>
 <and-expression>                ::= <equality-expression>
                                   | <and-expression> "&" <equality-expression>
 <equality-expression>           ::= <relational-expression>
                                   | <equality-expression> "==" <relational-expression>
                                   | <equality-expression> "!=" <relational-expression>
 <relational-expression>         ::= <shift-expression>
                                   | <relational-expression> "<"  <shift-expression>
                                   | <relational-expression> ">" <shift-expression>
                                   | <relational-expression> "<=" <shift-expression>
                                   | <relational-expression> ">=" <shift-expression>
 <shift-expression>              ::= <additive-expression>
                                   | <shift-expression> "<<" <additive-expression>
                                   | <shift-expression> ">>" <additive-expression>
 <additive-expression>           ::= <multiplicative-expression>
                                   | <additive-expression> "+" <multiplicative-expression>
                                   | <additive-expression> "-" <multiplicative-expression>
 <multiplicative-expression>     ::= <unary-expression>
                                   | <multiplicative-expression> "*" <unary-expression>
                                   | <multiplicative-expression> "/" <unary-expression>
                                   | <multiplicative-expression> "%" <unary-expression>
 <unary-expression>              ::= <postfix-expression>
                                   | "&" <unary-expression>
                                   | "+" <unary-expression>
                                   | "-" <unary-expression>
                                   | "~" <unary-expression>
                                   | "!" <unary-expression>
 <postfix-expression>            ::= <primary-expression>
                                   | <postfix-expression> "[" {<assignment-expression>}* "]"
                                   | <postfix-expression> "(" {<assignment-expression>}* ")"
                                   | <postfix-expression> "." <identifier>
 <primary-expression>            ::= <identifier>
                                   | <integer-literal>
                                   | <boolean-literal>
                                   | <floating-point-literal>
                                   | <string-literal>
                                   | "(" <comma-expression> ")"
 
 \endcode
 
 clang-format on
 */

namespace scatha::parse {

[[nodiscard]] SCATHA(API) ast::UniquePtr<ast::AbstractSyntaxTree> parse(utl::vector<Token> tokens, issue::SyntaxIssueHandler& issueHandler);

} // namespace scatha::parse

#endif // SCATHA_PARSER_PARSER2_H_
