#ifndef SCATHA_PARSE_PARSERIMPL_H_
#define SCATHA_PARSE_PARSERIMPL_H_

#include <concepts>

#include <utl/concepts.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Expected.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parse;

namespace {

struct Context {
    UniquePtr<ast::AbstractSyntaxTree> run();

    UniquePtr<ast::TranslationUnit> parseTranslationUnit();
    UniquePtr<ast::Declaration> parseExternalDeclaration();
    UniquePtr<ast::FunctionDefinition> parseFunctionDefinition();
    UniquePtr<ast::ParameterDeclaration> parseParameterDeclaration();
    UniquePtr<ast::StructDefinition> parseStructDefinition();
    UniquePtr<ast::VariableDeclaration> parseVariableDeclaration();
    UniquePtr<ast::VariableDeclaration> parseShortVariableDeclaration(
        std::optional<Token> declarator = std::nullopt);
    UniquePtr<ast::Statement> parseStatement();
    UniquePtr<ast::ExpressionStatement> parseExpressionStatement();
    UniquePtr<ast::EmptyStatement> parseEmptyStatement();
    UniquePtr<ast::CompoundStatement> parseCompoundStatement();
    UniquePtr<ast::ControlFlowStatement> parseControlFlowStatement();
    UniquePtr<ast::ReturnStatement> parseReturnStatement();
    UniquePtr<ast::IfStatement> parseIfStatement();
    UniquePtr<ast::WhileStatement> parseWhileStatement();
    UniquePtr<ast::DoWhileStatement> parseDoWhileStatement();
    UniquePtr<ast::ForStatement> parseForStatement();
    UniquePtr<ast::BreakStatement> parseBreakStatement();
    UniquePtr<ast::ContinueStatement> parseContinueStatement();

    // Expressions
    UniquePtr<ast::Expression> parseTypeExpression();
    UniquePtr<ast::ReferenceExpression> parseReferenceExpression();
    UniquePtr<ast::Expression> parseComma();
    UniquePtr<ast::Expression> parseAssignment();
    UniquePtr<ast::Expression> parseConditional();
    UniquePtr<ast::Expression> parseLogicalOr();
    UniquePtr<ast::Expression> parseLogicalAnd();
    UniquePtr<ast::Expression> parseInclusiveOr();
    UniquePtr<ast::Expression> parseExclusiveOr();
    UniquePtr<ast::Expression> parseAnd();
    UniquePtr<ast::Expression> parseEquality();
    UniquePtr<ast::Expression> parseRelational();
    UniquePtr<ast::Expression> parseShift();
    UniquePtr<ast::Expression> parseAdditive();
    UniquePtr<ast::Expression> parseMultiplicative();
    UniquePtr<ast::Expression> parseUnary();
    UniquePtr<ast::Expression> parsePostfix();
    UniquePtr<ast::Expression> parsePrimary();
    UniquePtr<ast::Identifier> parseIdentifier();
    UniquePtr<ast::IntegerLiteral> parseIntegerLiteral();
    UniquePtr<ast::BooleanLiteral> parseBooleanLiteral();
    UniquePtr<ast::FloatingPointLiteral> parseFloatingPointLiteral();
    UniquePtr<ast::StringLiteral> parseStringLiteral();

    // Helpers
    void pushExpectedExpression(Token const&);

    template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
    bool recover(std::pair<Cond, F>... retry);

    template <std::predicate... F>
    bool recover(std::pair<TokenKind, F>... retry);

    template <typename Expr>
    UniquePtr<Expr> parseFunctionCallLike(UniquePtr<ast::Expression> primary,
                                          TokenKind open,
                                          TokenKind close);

    template <typename List, typename DList = std::decay_t<List>>
    std::optional<DList> parseList(TokenKind open,
                                   TokenKind close,
                                   TokenKind delimiter,
                                   auto parseCallback);

    UniquePtr<ast::Subscript> parseSubscript(
        UniquePtr<ast::Expression> primary);
    UniquePtr<ast::FunctionCall> parseFunctionCall(
        UniquePtr<ast::Expression> primary);
    UniquePtr<ast::Expression> parseMemberAccess(
        UniquePtr<ast::Expression> primary);

    template <ast::BinaryOperator...>
    UniquePtr<ast::Expression> parseBinaryOperatorLTR(auto&& operand);

    template <ast::BinaryOperator...>
    UniquePtr<ast::Expression> parseBinaryOperatorRTL(auto&& parseOperand);

    void expectDelimiter(TokenKind delimiter);

    // Data
    TokenStream tokens;
    IssueHandler& issues;
};

} // namespace

#endif // SCATHA_PARSE_PARSERIMPL_H_
