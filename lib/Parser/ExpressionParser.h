#ifndef SCATHA_PARSER_EXPRESSIONPARSER_H_
#define SCATHA_PARSER_EXPRESSIONPARSER_H_

#include "AST/AST.h"
#include "Common/Allocator.h"
#include "Parser/TokenStream.h"
#include "Issue/IssueHandler.h"

namespace scatha::parse {

/**
 clang-format off
 
 MARK: Operator precedence
 \code
 
 +-----------+-------------------+-------------------------------+-------------------+
 |Precedence | Operator          | Description                   | Associativity     |
 +-----------+-------------------+-------------------------------+-------------------+
 |   1       | ()                | Function call                 | Left to right ->  |
 |           | []                | Subscript                     |                   |
 |           | .                 | Member access                 |                   |
 +-----------+-------------------+-------------------------------+-------------------+
 |   2       | +, -              | Unary plus and minus          | Right to left <-  |
 |           | !, ~              | Logical and bitwise NOT       |                   |
 |           | &                 | Address of                    |                   |
 +-----------+-------------------+-------------------------------+-------------------+
 |   3       | *, /, %           | Multiplication, division      | Left to right ->  |
 |           |                   | and remainder                 |                   |
 |   4       | +, -              | Addition and subtraction      |                   |
 |   5       | <<, >>            | Bitwise left and right shift  |                   |
 |   6       | <, <=, >, >=      | Relational operators          |                   |
 |   7       | ==, !=            | Equality operators            |                   |
 |   8       | &                 | Bitwise AND                   |                   |
 |   9       | ^                 | Bitwise XOR                   |                   |
 |  10       | |                 | Bitwise OR                    |                   |
 |  11       | &&                | Logical AND                   |                   |
 |  12       | ||                | Logical OR                    |                   |
 +-----------+-------------------+-------------------------------+-------------------+
 |  13       | ?:                | Conditional                   | Right to left <-  |
 |           | =, +=, -=         | Assignment                    | Right to left <-  |
 |           | *=, /=, %=        |                               |                   |
 |           | <<=, >>=,         |                               |                   |
 |           | &=, |=,           |                               |                   |
 +-----------+-------------------+-------------------------------+-------------------+
 |  14       | ,                 | Comma operator                | Left to right ->  |
 +-----------+-------------------+-------------------------------+-------------------+

 \endcode
 
 MARK: Grammar
 \code
 
 <comma-expression>              ::= <assignment-expression>
                                   | <comma-expression> "," <assignment-expression>
 <assignment-expression>         ::= <conditional-expression>
                                   | <conditional-expression> "=, *=, ..." <assignment-expression>
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
                                   | <postfix-expression> "[" {<assignment-expression>}+ "]"
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

class SCATHA(API) ExpressionParser {
public:
    explicit ExpressionParser(TokenStream& tokens, issue::ParsingIssueHandler& iss): tokens(tokens), iss(iss) {}

    ast::UniquePtr<ast::Expression> parseExpression();

    ast::UniquePtr<ast::Expression> parseComma();
    ast::UniquePtr<ast::Expression> parseAssignment();
    ast::UniquePtr<ast::Expression> parseConditional();
    ast::UniquePtr<ast::Expression> parseLogicalOr();
    ast::UniquePtr<ast::Expression> parseLogicalAnd();
    ast::UniquePtr<ast::Expression> parseInclusiveOr();
    ast::UniquePtr<ast::Expression> parseExclusiveOr();
    ast::UniquePtr<ast::Expression> parseAnd();
    ast::UniquePtr<ast::Expression> parseEquality();
    ast::UniquePtr<ast::Expression> parseRelational();
    ast::UniquePtr<ast::Expression> parseShift();
    ast::UniquePtr<ast::Expression> parseAdditive();
    ast::UniquePtr<ast::Expression> parseMultiplicative();
    ast::UniquePtr<ast::Expression> parseUnary();
    ast::UniquePtr<ast::Expression> parsePostfix();
    ast::UniquePtr<ast::Expression> parsePrimary();
    ast::UniquePtr<ast::Identifier> parseIdentifier();
    ast::UniquePtr<ast::IntegerLiteral> parseIntegerLiteral();
    ast::UniquePtr<ast::BooleanLiteral> parseBooleanLiteral();
    ast::UniquePtr<ast::FloatingPointLiteral> parseFloatingPointLiteral();
    ast::UniquePtr<ast::StringLiteral> parseStringLiteral();

private:
    template <typename Expr>
    ast::UniquePtr<Expr> parseFunctionCallLike(ast::UniquePtr<ast::Expression> primary,
                                               std::string_view open,
                                               std::string_view close);
    ast::UniquePtr<ast::Subscript> parseSubscript(ast::UniquePtr<ast::Expression> primary);
    ast::UniquePtr<ast::FunctionCall> parseFunctionCall(ast::UniquePtr<ast::Expression> primary);
    ast::UniquePtr<ast::Expression> parseMemberAccess(ast::UniquePtr<ast::Expression> primary);

    template <ast::BinaryOperator...>
    ast::UniquePtr<ast::Expression> parseBinaryOperatorLTR(auto&& operand);

    template <ast::BinaryOperator...>
    ast::UniquePtr<ast::Expression> parseBinaryOperatorRTL(auto&& parseOperand);

private:
    TokenStream& tokens;
    issue::ParsingIssueHandler& iss;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_EXPRESSIONPARSER_H_
