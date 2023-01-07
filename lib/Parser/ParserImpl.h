/// This header is not meant to expose the internal parser interface but to separate it from the implementation file.
/// Thus the weird include guard to prevent multiple include.

#ifdef SCATHA_PARSE_PARSERIMPL_H_
#error Include this file only once
#endif // SCATHA_PARSE_PARSERIMPL_H_
#define SCATHA_PARSE_PARSERIMPL_H_

#include <concepts>

#include <utl/concepts.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Common/Keyword.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parse;

namespace {

struct Context {
    ast::UniquePtr<ast::AbstractSyntaxTree> run();

    ast::UniquePtr<ast::TranslationUnit> parseTranslationUnit();
    ast::UniquePtr<ast::Declaration> parseExternalDeclaration();
    ast::UniquePtr<ast::FunctionDefinition> parseFunctionDefinition();
    ast::UniquePtr<ast::ParameterDeclaration> parseParameterDeclaration();
    ast::UniquePtr<ast::StructDefinition> parseStructDefinition();
    ast::UniquePtr<ast::VariableDeclaration> parseVariableDeclaration();
    ast::UniquePtr<ast::VariableDeclaration> parseShortVariableDeclaration(
        std::optional<Token> declarator = std::nullopt);
    ast::UniquePtr<ast::Statement> parseStatement();
    ast::UniquePtr<ast::ExpressionStatement> parseExpressionStatement();
    ast::UniquePtr<ast::EmptyStatement> parseEmptyStatement();
    ast::UniquePtr<ast::CompoundStatement> parseCompoundStatement();
    ast::UniquePtr<ast::ControlFlowStatement> parseControlFlowStatement();
    ast::UniquePtr<ast::ReturnStatement> parseReturnStatement();
    ast::UniquePtr<ast::IfStatement> parseIfStatement();
    ast::UniquePtr<ast::WhileStatement> parseWhileStatement();
    ast::UniquePtr<ast::DoWhileStatement> parseDoWhileStatement();
    ast::UniquePtr<ast::ForStatement> parseForStatement();

    // Expressions

    ast::UniquePtr<ast::Expression> parseComma();
    ast::UniquePtr<ast::Expression> parseAssignment();
    ast::UniquePtr<ast::Expression> parseTypeExpression() { return parseConditional(); } // Convenience wrapper
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

    // Helpers
    //    void pushExpectedExpressionBefore(SourceLocation);
    //    void pushExpectedExpressionAfter(SourceLocation);
    void pushExpectedExpression(Token const&);
    ///
    //    void panic();

    template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
    bool recover(std::pair<Cond, F>... retry);

    template <std::predicate... F>
    bool recover(std::pair<std::string_view, F>... retry);

    template <typename Expr>
    ast::UniquePtr<Expr> parseFunctionCallLike(ast::UniquePtr<ast::Expression> primary,
                                               std::string_view open,
                                               std::string_view close);

    template <typename List, typename DList = std::decay_t<List>>
    std::optional<DList> parseList(std::string_view open,
                                   std::string_view close,
                                   std::string_view delimiter,
                                   auto parseCallback);

    ast::UniquePtr<ast::Subscript> parseSubscript(ast::UniquePtr<ast::Expression> primary);
    ast::UniquePtr<ast::FunctionCall> parseFunctionCall(ast::UniquePtr<ast::Expression> primary);
    ast::UniquePtr<ast::Expression> parseMemberAccess(ast::UniquePtr<ast::Expression> primary);

    template <ast::BinaryOperator...>
    ast::UniquePtr<ast::Expression> parseBinaryOperatorLTR(auto&& operand);

    template <ast::BinaryOperator...>
    ast::UniquePtr<ast::Expression> parseBinaryOperatorRTL(auto&& parseOperand);

    void expectDelimiter(std::string_view delimiter);

    // Data

    TokenStream tokens;
    issue::SyntaxIssueHandler& iss;
};

} // namespace
