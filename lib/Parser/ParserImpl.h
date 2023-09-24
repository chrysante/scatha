#ifndef SCATHA_PARSE_PARSERIMPL_H_
#define SCATHA_PARSE_PARSERIMPL_H_

#include <concepts>

#include <utl/concepts.hpp>
#include <utl/function_view.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Expected.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parser;

namespace {

struct Context {
    UniquePtr<ast::ASTNode> run();

    UniquePtr<ast::TranslationUnit> parseTranslationUnit();
    UniquePtr<ast::Declaration> parseExternalDeclaration();
    UniquePtr<ast::FunctionDefinition> parseFunctionDefinition();
    UniquePtr<ast::ParameterDeclaration> parseParameterDeclaration(
        size_t index);
    UniquePtr<ast::StructDefinition> parseStructDefinition();
    UniquePtr<ast::VariableDeclaration> parseVariableDeclaration();
    UniquePtr<ast::VariableDeclaration> parseShortVariableDeclaration(
        sema::Mutability mutability,
        std::optional<Token> declarator = std::nullopt);
    UniquePtr<ast::Statement> parseStatement();
    UniquePtr<ast::ExpressionStatement> parseExpressionStatement();
    UniquePtr<ast::EmptyStatement> parseEmptyStatement();
    UniquePtr<ast::CompoundStatement> parseCompoundStatement();
    UniquePtr<ast::ControlFlowStatement> parseControlFlowStatement();
    UniquePtr<ast::ReturnStatement> parseReturnStatement();
    UniquePtr<ast::IfStatement> parseIfStatement();
    UniquePtr<ast::LoopStatement> parseWhileStatement();
    UniquePtr<ast::LoopStatement> parseDoWhileStatement();
    UniquePtr<ast::LoopStatement> parseForStatement();
    UniquePtr<ast::JumpStatement> parseJumpStatement();

    // Expressions
    UniquePtr<ast::Expression> parseTypeExpression();
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
    UniquePtr<ast::Expression> parsePrefix();
    UniquePtr<ast::Expression> parsePostfix();
    UniquePtr<ast::Expression> parseGeneric();
    UniquePtr<ast::Expression> parsePrimary();
    UniquePtr<ast::Identifier> parseID();
    UniquePtr<ast::Identifier> parseExtID();
    UniquePtr<ast::Literal> parseLiteral();

    // Helpers
    sema::Mutability eatMut();

    void pushExpectedExpression(Token const&);

    template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
    bool recover(std::pair<Cond, F>... retry);

    template <std::predicate... F>
    bool recover(std::pair<TokenKind, F>... retry);

    template <typename Expr>
    UniquePtr<Expr> parseFunctionCallLike(UniquePtr<ast::Expression> primary,
                                          TokenKind open,
                                          TokenKind close);

    template <typename Expr>
    UniquePtr<Expr> parseFunctionCallLike(UniquePtr<ast::Expression> primary,
                                          TokenKind open,
                                          TokenKind close,
                                          auto parseCallback);

    template <typename List, typename DList = std::decay_t<List>>
    std::optional<DList> parseList(TokenKind open,
                                   TokenKind close,
                                   TokenKind delimiter,
                                   auto parseCallback);

    UniquePtr<ast::UnaryExpression> parseUnaryPostfix(
        ast::UnaryOperator op, UniquePtr<ast::Expression> primary);
    UniquePtr<ast::CallLike> parseSubscript(UniquePtr<ast::Expression> primary);
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
