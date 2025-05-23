// clang-format off

///
/// ## Grammar
///
/// ```
/// <translation-unit>              ::= {<global-statement>}*
/// <global-statement>              ::= <import-statement> | <declaration>
/// <declaration>                   ::= [<access-spec>] <function-definition>
///                                   | [<access-spec>] <struct-definition>
///                                   | [<access-spec>] <protocol-definition>
///                                   | [<access-spec>] <variable-declaration>
/// <function-definition>           ::= "fn" <identifier>
///                                          "(" {<parameter-declaration>}* ")"
///                                          ["->" <type-expression>]
///                                          <compound-statement>
/// <parameter-declaration>         ::= <identifier> ":" <type-expression>
///                                   | "this"
///                                   | "& this"
///                                   | "& mut this"
/// <struct-definition>             ::= "struct" <record-definition>
/// <protocol-definition>           ::= "protocol" <record-definition>
/// <record-definition>             ::= <identifier> [":" <base-class-decl> 
///                                                   ("," <base-class-decl>)*]
///                                                  (";" | <compound-statement>)
/// <base-class-decl>               ::= [<access-spec>] <type-expression>
/// <variable-declaration>          ::= ("var"|"let") <short-var-declaration>
/// <short-var-declaration>         ::= <identifier> [":" <type-expression>]
///                                                  ["=" <assignment-expression>] ";"
/// <statement>                     ::= <declaration>
///                                   | <control-flow-statement>
///                                   | <compound-statement>
///                                   | <import-statement>
///                                   | <expression-statement>
///                                   | ";"
/// <import-statement>              ::= ("import" | "use") <postfix-expression>
/// <expression-statement>          ::= <comma-expression> ";"
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
/// <type-expression>               ::= <prefix-expression>
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
/// <multiplicative-expression>     ::= <cast-expression>
///                                   | <multiplicative-expression> "*" <prefix-expression>
///                                   | <multiplicative-expression> "/" <prefix-expression>
///                                   | <multiplicative-expression> "%" <prefix-expression>
/// <cast-expression>               ::= <prefix-expression>
///                                   | <cast-expression> "as" <type-expression>
/// <prefix-expression>              ::= <postfix-expression>
///                                   | "+" <prefix-expression>
///                                   | "-" <prefix-expression>
///                                   | "~" <prefix-expression>
///                                   | "!" <prefix-expression>
///                                   | "++" <prefix-expression>
///                                   | "--" <prefix-expression>
///                                   | "*" <qualifier>* <prefix-expression>
///                                   | "&" <qualifier>* <prefix-expression>
///                                   | "move" <prefix-expression>
///                                   | "unique" <prefix-expression>
/// <postfix-expression>            ::= <generic-expression>
///                                   | <postfix-expression> "++"
///                                   | <postfix-expression> "--"
///                                   | <postfix-expression> "[" {<assignment-expression>} ":" {<assignment-expression>} "]"
///                                   | <postfix-expression> "[" {<assignment-expression>}* "]"
///                                   | <postfix-expression> "(" {<assignment-expression>}* ")"
///                                   | <postfix-expression> "." <identifier>
/// <generic-expression>            ::= <primary-expression>
///                                   | <generic-id> "<" {<additive-expression>}* ">"
/// <fstring>                       ::= <primary-expression>
///                                   | <fstring-lit-begin> <assignment-expression> [<fstring-continue>] <fstring-lit-end>
/// <fstring-continue>              ::= <fstring-lit-continue> <assignment-expression> [<fstring-continue>]
/// <primary-expression>            ::= <identifier>
///                                   | <integer-literal>
///                                   | <boolean-literal>
///                                   | <floating-point-literal>
///                                   | "this"
///                                   | <string-literal>
///                                   | "(" <comma-expression> ")"
///                                   | "[" {<conditional-expression>}* "]"
/// <access-spec>                   ::= "public" | "private"
/// <qualifier>                     ::= "mut" | "dyn"
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

#include "Parser/Parser.h"

#include <concepts>
#include <optional>
#include <tuple>
#include <variant>

#include <utl/concepts.hpp>
#include <utl/function_view.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Expected.h"
#include "Parser/BracketCorrection.h"
#include "Parser/Lexer.h"
#include "Parser/Panic.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parser;
using namespace ranges::views;
using enum TokenKind;

namespace {

using Qualifier =
    std::variant<std::monostate, sema::Mutability, sema::PointerBindMode>;

struct QualifierSet {
    sema::Mutability mut;
    sema::PointerBindMode bindMode;
};

struct Context {
    TokenStream tokens;
    std::string filename;
    IssueHandler& issues;

    UniquePtr<ast::SourceFile> run();

    UniquePtr<ast::SourceFile> parseSourceFile(std::string filename);
    UniquePtr<ast::Statement> parseGlobalStatement();
    UniquePtr<ast::Declaration> parseDeclaration();
    UniquePtr<ast::FunctionDefinition> parseFunctionDefinition(
        ast::SpecifierList specList);
    UniquePtr<ast::ParameterDeclaration> parseParameterDeclaration(
        size_t index);
    UniquePtr<ast::RecordDefinition> parseRecordDefinition(
        ast::SpecifierList specList);
    UniquePtr<ast::BaseClassDeclaration> parseBaseClass();
    UniquePtr<ast::VariableDeclaration> parseVariableDeclaration(
        ast::SpecifierList specList);
    UniquePtr<ast::VariableDeclaration> parseShortVariableDeclaration(
        ast::SpecifierList specList, sema::Mutability mutability,
        std::optional<Token> declarator = std::nullopt);
    UniquePtr<ast::Statement> parseStatement();
    UniquePtr<ast::ImportStatement> parseImportStatement();
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
    UniquePtr<ast::Expression> parseCast();
    UniquePtr<ast::Expression> parsePrefix();
    UniquePtr<ast::Expression> parsePostfix();
    UniquePtr<ast::Expression> parseGeneric();
    UniquePtr<ast::Expression> parseFString();
    UniquePtr<ast::Expression> parsePrimary();
    UniquePtr<ast::Identifier> parseID();
    UniquePtr<ast::Identifier> parseExtID();
    UniquePtr<ast::Literal> parseLiteral();

    // Helpers
    sema::Mutability parseMut(SourceRange& sr);
    Qualifier parseQualifier(SourceRange& sr);
    QualifierSet parseQualifierSet(SourceRange sr);
    ast::SpecifierList parseSpecList();

    void pushExpectedExpression(Token const&);

    template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
    bool recover(std::pair<Cond, F>... retry);

    template <std::predicate... F>
    bool recover(std::pair<TokenKind, F>... retry);

    template <typename Expr>
    UniquePtr<Expr> parseFunctionCallLike(UniquePtr<ast::Expression> primary,
                                          TokenKind open, TokenKind close);

    template <typename Expr>
    UniquePtr<Expr> parseFunctionCallLike(UniquePtr<ast::Expression> primary,
                                          TokenKind open, TokenKind close,
                                          auto parseCallback);

    template <typename List, typename DList = std::decay_t<List>>
    std::optional<DList> parseList(TokenKind open, TokenKind close,
                                   TokenKind delimiter, auto parseCallback);

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

    bool expectDelimiter(TokenKind delimiter);
};

} // namespace

UniquePtr<ast::ASTNode> parser::parse(std::span<SourceFile const> sourceFiles,
                                      IssueHandler& issueHandler) {
    utl::small_vector<UniquePtr<ast::SourceFile>> parsedFiles;
    for (auto [index, file]: sourceFiles | ranges::views::enumerate) {
        auto tokens = lex(file.text(), issueHandler, index);
        if (issueHandler.haveErrors()) {
            return nullptr;
        }
        bracketCorrection(tokens, issueHandler);
        Context ctx{ .tokens = TokenStream(std::move(tokens)),
                     .filename = file.path().string(),
                     .issues = issueHandler };
        parsedFiles.push_back(ctx.run());
    }
    return allocate<ast::TranslationUnit>(std::move(parsedFiles));
}

UniquePtr<ast::ASTNode> parser::parse(std::string_view source,
                                      IssueHandler& issueHandler) {
    utl::small_vector<SourceFile, 1> files = { SourceFile::make(
        std::string(source)) };
    return parse(files, issueHandler);
}

UniquePtr<ast::SourceFile> Context::run() { return parseSourceFile(filename); }

UniquePtr<ast::SourceFile> Context::parseSourceFile(std::string filename) {
    utl::small_vector<UniquePtr<ast::Statement>> stmts;
    while (true) {
        Token const token = tokens.peek();
        if (token.kind() == EndOfFile) {
            break;
        }
        if (auto stmt = parseGlobalStatement()) {
            stmts.push_back(std::move(stmt));
            continue;
        }
        if (tokens.peek().kind() == EndOfFile) {
            break;
        }
        if (isDeclarator(tokens.peek().kind())) {
            /// \Note Here we `eat()` a token because `panic()` will not
            /// advance past the next declarator in this scope, so we would
            /// be stuck here
            issues.push<UnexpectedDeclarator>(tokens.eat());
        }
        else {
            issues.push<ExpectedDeclarator>(tokens.peek());
        }
        panic(tokens);
    }
    return allocate<ast::SourceFile>(std::move(filename), std::move(stmts));
}

UniquePtr<ast::Statement> Context::parseGlobalStatement() {
    if (auto importStmt = parseImportStatement()) {
        return importStmt;
    }
    if (auto decl = parseDeclaration()) {
        return decl;
    }
    return nullptr;
}

static sema::ImportKind toImportKind(TokenKind kind) {
    switch (kind) {
    case Import:
        return sema::ImportKind::Scoped;
    case Use:
        return sema::ImportKind::Unscoped;
    default:
        SC_UNREACHABLE();
    }
}

UniquePtr<ast::ImportStatement> Context::parseImportStatement() {
    auto token = tokens.peek();
    if (token.kind() != Import && token.kind() != Use) {
        return nullptr;
    }
    tokens.eat();
    auto expr = parsePostfix();
    if (!expr) {
        pushExpectedExpression(tokens.peek());
    }
    expectDelimiter(Semicolon);
    return allocate<ast::ImportStatement>(token.sourceRange(), std::move(expr),
                                          toImportKind(token.kind()));
}

UniquePtr<ast::Declaration> Context::parseDeclaration() {
    auto specList = parseSpecList();
    if (auto funcDef = parseFunctionDefinition(specList)) {
        return funcDef;
    }
    if (auto recordDef = parseRecordDefinition(specList)) {
        return recordDef;
    }
    if (auto varDef = parseVariableDeclaration(specList)) {
        return varDef;
    }
    return nullptr;
}

template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
bool Context::recover(std::pair<Cond, F>... retry) {
    /// Eat a few tokens and see if we can find a list.
    /// 5 is pretty arbitrary here.
    int const maxDiscardedTokens = 5;
    for (int i = 0; i < maxDiscardedTokens; ++i) {
        Token const& next = tokens.peek();
        bool success = false;
        ([&](auto const& cond, auto const& callback) {
            if (std::invoke(cond, next)) {
                success = std::invoke(callback);
                return true;
            }
            return false;
        }(retry.first, retry.second) || ...);
        if (success) {
            return true;
        }
        tokens.eat();
    }
    /// Here we assume that `panic()` gets us past the function body.
    panic(tokens);
    return false;
}

template <std::predicate... F>
bool Context::recover(std::pair<TokenKind, F>... retry) {
    return recover(std::pair{ [&](Token const& token) {
        return token.kind() == retry.first;
    }, retry.second }...);
}

UniquePtr<ast::FunctionDefinition> Context::parseFunctionDefinition(
    ast::SpecifierList specList) {
    Token const declarator = tokens.peek();
    if (declarator.kind() != Function) {
        return nullptr;
    }
    tokens.eat();
    auto identifier = parseExtID();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.peek());
        bool const recovered = recover(std::pair{ [&](Token const&) {
            return true;
        }, [&] { return bool(identifier = parseExtID()); } });
        if (!recovered) {
            return nullptr;
        }
    }
    /// Parse parameters
    using ParamListType =
        utl::small_vector<UniquePtr<ast::ParameterDeclaration>>;
    auto parseList = [this] {
        return this->parseList<ParamListType>(OpenParan, CloseParan, Comma,
                                              [this,
                                               index = size_t{ 0 }]() mutable {
            return parseParameterDeclaration(index++);
        });
    };
    auto params = parseList();
    if (!params) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenParan);
        // clang-format off
        bool const recovered = recover(std::pair{ OpenBrace, [&] {
            /// User forgot to specify parameter list?
            /// Go on with parsing the body.
            params = ParamListType{};
            return true;
        } }, std::pair{ OpenParan, [&] {
            /// Try parsing parameter list again.
            return bool(params = parseList());
        }});
        // clang-format on
        if (!recovered) {
            return nullptr;
        }
    }
    auto closeParan = tokens.current();
    UniquePtr<ast::Expression> returnTypeExpr;
    if (Token const arrow = tokens.peek(); arrow.kind() == Arrow) {
        tokens.eat();
        returnTypeExpr = parseTypeExpression();
        if (!returnTypeExpr) {
            pushExpectedExpression(tokens.peek());
        }
    }
    auto sourceRange =
        merge(declarator.sourceRange(), closeParan.sourceRange());
    if (auto delim = tokens.peek(); delim.kind() == Semicolon) {
        tokens.eat();
        return allocate<ast::FunctionDefinition>(sourceRange, specList,
                                                 std::move(identifier),
                                                 std::move(*params),
                                                 std::move(returnTypeExpr),
                                                 nullptr);
    }
    auto body = parseCompoundStatement();
    if (!body) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
        /// Eat a few tokens and see if we can find a compound statement.
        /// 5 is pretty arbitrary here.
        bool recovered = recover(std::pair(Semicolon, [&] { return true; }),
                                 std::pair(OpenBrace, [&] {
            return bool(body = parseCompoundStatement());
        }));
        if (!recovered) {
            return nullptr;
        }
    }
    return allocate<ast::FunctionDefinition>(sourceRange, specList,
                                             std::move(identifier),
                                             std::move(*params),
                                             std::move(returnTypeExpr),
                                             std::move(body));
}

UniquePtr<ast::ParameterDeclaration> Context::parseParameterDeclaration(
    size_t index) {
    SourceRange thisSourceRange;
    auto thisQualSet = parseQualifierSet(thisSourceRange);
    Token idToken = tokens.peek();
    if (idToken.kind() == This) {
        tokens.eat();
        return allocate<ast::ThisParameter>(index, thisQualSet.mut,
                                            thisQualSet.bindMode,
                                            /* isRef = */ false,
                                            merge(thisSourceRange,
                                                  idToken.sourceRange()));
    }
    if (idToken.kind() == BitAnd) {
        tokens.eat();
        using enum sema::Mutability;
        auto thisQualSet = parseQualifierSet(thisSourceRange);
        if (tokens.peek().kind() != This) {
            return nullptr;
        }
        auto thisToken = tokens.eat();
        thisSourceRange = merge(thisSourceRange, idToken.sourceRange(),
                                thisToken.sourceRange());
        return allocate<ast::ThisParameter>(index, thisQualSet.mut,
                                            thisQualSet.bindMode,
                                            /* isRef = */ true,
                                            thisSourceRange);
    }
    auto identifier = parseID();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(idToken);
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.kind() == Colon) {
                break;
            }
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    Token const colon = tokens.peek();
    if (colon.kind() != Colon) {
        issues.push<UnqualifiedID>(colon, Colon);
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    else {
        tokens.eat();
    }
    SourceRange typeSourceRange;
    auto mutQual = parseMut(typeSourceRange);
    auto typeExpr = parseTypeExpression();
    if (!typeExpr) {
        pushExpectedExpression(tokens.peek());
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    return allocate<ast::ParameterDeclaration>(index, mutQual,
                                               std::move(identifier),
                                               std::move(typeExpr));
}

UniquePtr<ast::RecordDefinition> Context::parseRecordDefinition(
    ast::SpecifierList specList) {
    Token const& declarator = tokens.peek();
    if (declarator.kind() != Struct && declarator.kind() != Protocol) {
        return nullptr;
    }
    tokens.eat();
    auto identifier = parseID();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.peek());
        bool recovered = recover(std::pair(OpenBrace, [] { return true; }));
        if (!recovered) {
            return nullptr;
        }
    }
    utl::small_vector<UniquePtr<ast::BaseClassDeclaration>> baseClasses;
    if (tokens.peek().kind() == Colon) {
        tokens.eat();
        while (true) {
            auto base = parseBaseClass();
            baseClasses.push_back(std::move(base));
            Token const& next = tokens.peek();
            if (next.kind() == Comma) {
                tokens.eat();
                continue;
            }
            if (next.kind() != OpenBrace && next.kind() != Semicolon) {
                issues.push<UnqualifiedID>(next, OpenBrace);
            }
            break;
        }
    }
    auto body = [&]() -> UniquePtr<ast::CompoundStatement> {
        if (tokens.peek().kind() == Semicolon) {
            auto SR = tokens.peek().sourceRange();
            tokens.eat();
            return allocate<ast::CompoundStatement>(
                SR, utl::small_vector<UniquePtr<ast::Statement>>{});
        }
        auto body = parseCompoundStatement();
        if (!body) {
            SC_UNIMPLEMENTED(); // Handle issue
        }
        return body;
    }();
    switch (declarator.kind()) {
    case Struct:
        return allocate<ast::StructDefinition>(declarator.sourceRange(),
                                               specList, std::move(identifier),
                                               std::move(body),
                                               std::move(baseClasses));
    case Protocol:
        return allocate<ast::ProtocolDefinition>(declarator.sourceRange(),
                                                 specList,
                                                 std::move(identifier),
                                                 std::move(body),
                                                 std::move(baseClasses));
    default:
        SC_UNREACHABLE();
    }
}

UniquePtr<ast::BaseClassDeclaration> Context::parseBaseClass() {
    auto const& tok = tokens.peek();
    auto specList = parseSpecList();
    auto type = parseTypeExpression();
    auto SR = merge(tok.sourceRange(), type->sourceRange());
    return allocate<ast::BaseClassDeclaration>(specList, SR, std::move(type));
}

UniquePtr<ast::VariableDeclaration> Context::parseVariableDeclaration(
    ast::SpecifierList specList) {
    Token const declarator = tokens.peek();
    if (declarator.kind() != Var && declarator.kind() != Let) {
        return nullptr;
    }
    tokens.eat();
    using enum sema::Mutability;
    auto mut = declarator.kind() == Var ? Mutable : Const;
    auto result = parseShortVariableDeclaration(specList, mut, declarator);
    result->setDirectSourceRange(declarator.sourceRange());
    return result;
}

UniquePtr<ast::VariableDeclaration> Context::parseShortVariableDeclaration(
    ast::SpecifierList specList, sema::Mutability mutability,
    std::optional<Token> declarator) {
    auto identifier = parseID();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.current());
        panic(tokens);
    }
    UniquePtr<ast::Expression> typeExpr, initExpr;
    if (Token const colon = tokens.peek(); colon.kind() == Colon) {
        tokens.eat();
        typeExpr = parseTypeExpression();
        if (!typeExpr) {
            issues.push<ExpectedExpression>(tokens.current());
            panic(tokens);
        }
    }
    if (Token const assign = tokens.peek(); assign.kind() == Assign) {
        tokens.eat();
        initExpr = parseAssignment();
        if (!initExpr) {
            issues.push<ExpectedExpression>(tokens.current());
            panic(tokens);
        }
    }
    if (Token const semicolon = tokens.peek(); semicolon.kind() != Semicolon) {
        issues.push<ExpectedDelimiter>(tokens.current());
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    auto sourceRange = declarator ? declarator->sourceRange() :
                       identifier ? identifier->sourceRange() :
                                    tokens.current().sourceRange();
    return allocate<ast::VariableDeclaration>(specList, mutability, sourceRange,
                                              std::move(identifier),
                                              std::move(typeExpr),
                                              std::move(initExpr));
}

UniquePtr<ast::Statement> Context::parseStatement() {
    if (auto extDecl = parseDeclaration()) {
        return extDecl;
    }
    if (auto controlFlowStatement = parseControlFlowStatement()) {
        return controlFlowStatement;
    }
    if (auto breakStatement = parseJumpStatement()) {
        return breakStatement;
    }
    if (auto block = parseCompoundStatement()) {
        return block;
    }
    if (auto importStmt = parseImportStatement()) {
        return importStmt;
    }
    if (auto expressionStatement = parseExpressionStatement()) {
        return expressionStatement;
    }
    if (auto emptyStatement = parseEmptyStatement()) {
        return emptyStatement;
    }
    return nullptr;
}

UniquePtr<ast::ExpressionStatement> Context::parseExpressionStatement() {
    auto expression = parseComma();
    if (!expression) {
        return nullptr;
    }
    expectDelimiter(Semicolon);
    return allocate<ast::ExpressionStatement>(std::move(expression));
}

UniquePtr<ast::CompoundStatement> Context::parseCompoundStatement() {
    Token const openBrace = tokens.peek();
    if (openBrace.kind() != OpenBrace) {
        return nullptr;
    }
    tokens.eat();
    utl::small_vector<UniquePtr<ast::Statement>> statements;
    while (true) {
        /// This mechanism checks whether a failed statement parse has eaten any
        /// tokens. If it hasn't we eat one ourselves and try again.
        auto const lastIndex = tokens.index();
        Token const next = tokens.peek();
        if (next.kind() == CloseBrace) {
            tokens.eat();
            break;
        }
        if (auto statement = parseStatement()) {
            statements.push_back(std::move(statement));
            continue;
        }
        if (tokens.peek().kind() == EndOfFile) {
            issues.push<UnqualifiedID>(tokens.eat(), CloseBrace);
            break;
        }
        if (tokens.index() == lastIndex) {
            /// If we can't parse a statement, eat one token and try again.
            issues.push<UnqualifiedID>(tokens.eat(), CloseBrace);
        }
    }
    return allocate<ast::CompoundStatement>(openBrace.sourceRange(),
                                            std::move(statements));
}

UniquePtr<ast::ControlFlowStatement> Context::parseControlFlowStatement() {
    if (auto returnStatement = parseReturnStatement()) {
        return returnStatement;
    }
    if (auto ifStatement = parseIfStatement()) {
        return ifStatement;
    }
    if (auto loop = parseWhileStatement()) {
        return loop;
    }
    if (auto loop = parseDoWhileStatement()) {
        return loop;
    }
    if (auto loop = parseForStatement()) {
        return loop;
    }
    return nullptr;
}

UniquePtr<ast::ReturnStatement> Context::parseReturnStatement() {
    Token const returnToken = tokens.peek();
    if (returnToken.kind() != Return) {
        return nullptr;
    }
    tokens.eat();
    /// May be null in case of void return statement.
    auto expression = parseComma();
    expectDelimiter(Semicolon);
    return allocate<ast::ReturnStatement>(returnToken.sourceRange(),
                                          std::move(expression));
}

UniquePtr<ast::IfStatement> Context::parseIfStatement() {
    Token const ifToken = tokens.peek();
    if (ifToken.kind() != If) {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto ifBlock = parseCompoundStatement();
    if (!ifBlock) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
        SC_UNIMPLEMENTED();
    }
    auto elseBlock = [&]() -> UniquePtr<ast::Statement> {
        Token const elseToken = tokens.peek();
        if (elseToken.kind() != Else) {
            return nullptr;
        }
        tokens.eat();
        if (auto elseBlock = parseIfStatement()) {
            return elseBlock;
        }
        if (auto elseBlock = parseCompoundStatement()) {
            return elseBlock;
        }
        Token const curr = tokens.current();
        Token const next = tokens.peek();
        SC_UNIMPLEMENTED(); // Handle issue
        return nullptr;
    }();
    return allocate<ast::IfStatement>(ifToken.sourceRange(), std::move(cond),
                                      std::move(ifBlock), std::move(elseBlock));
}

UniquePtr<ast::LoopStatement> Context::parseWhileStatement() {
    Token const whileToken = tokens.peek();
    if (whileToken.kind() != While) {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto block = parseCompoundStatement();
    if (!block) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    return allocate<ast::LoopStatement>(whileToken.sourceRange(),
                                        ast::LoopKind::While, nullptr,
                                        std::move(cond), nullptr,
                                        std::move(block));
}

UniquePtr<ast::LoopStatement> Context::parseDoWhileStatement() {
    Token const doToken = tokens.peek();
    if (doToken.kind() != Do) {
        return nullptr;
    }
    tokens.eat();
    auto block = parseCompoundStatement();
    if (!block) {
        // TODO: Push a better issue here
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    Token const whileToken = tokens.peek();
    if (whileToken.kind() != While) {
        issues.push<UnqualifiedID>(whileToken, While);
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    Token const delimToken = tokens.peek();
    if (delimToken.kind() != Semicolon) {
        issues.push<UnqualifiedID>(whileToken, Semicolon);
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    return allocate<ast::LoopStatement>(doToken.sourceRange(),
                                        ast::LoopKind::DoWhile, nullptr,
                                        std::move(cond), nullptr,
                                        std::move(block));
}

UniquePtr<ast::LoopStatement> Context::parseForStatement() {
    Token const forToken = tokens.peek();
    if (forToken.kind() != For) {
        return nullptr;
    }
    tokens.eat();
    using enum sema::Mutability;
    auto varDecl = parseShortVariableDeclaration({}, Mutable, forToken);
    if (!varDecl) {
        // TODO: This should be ExpectedDeclaration or something...
        pushExpectedExpression(tokens.peek());
        return nullptr;
    }
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    if (tokens.peek().kind() != Semicolon) {
        expectDelimiter(Semicolon);
        return nullptr;
    }
    tokens.eat();
    auto inc = parseComma();
    if (!inc) {
        pushExpectedExpression(tokens.peek());
    }
    auto block = parseCompoundStatement();
    if (!block) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    return allocate<ast::LoopStatement>(forToken.sourceRange(),
                                        ast::LoopKind::For, std::move(varDecl),
                                        std::move(cond), std::move(inc),
                                        std::move(block));
}

static std::optional<ast::JumpStatement::Kind> toJumpKind(Token token) {
    switch (token.kind()) {
    case Break:
        return ast::JumpStatement::Break;
    case Continue:
        return ast::JumpStatement::Continue;
    default:
        return std::nullopt;
    }
}

UniquePtr<ast::JumpStatement> Context::parseJumpStatement() {
    Token const token = tokens.peek();
    auto const jumpKind = toJumpKind(token);
    if (!jumpKind) {
        return nullptr;
    }
    tokens.eat();
    expectDelimiter(Semicolon);
    return allocate<ast::JumpStatement>(*jumpKind, token.sourceRange());
}

UniquePtr<ast::EmptyStatement> Context::parseEmptyStatement() {
    Token const delim = tokens.peek();
    if (delim.kind() != Semicolon) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::EmptyStatement>(delim.sourceRange());
}

// MARK: - Expressions

UniquePtr<ast::Expression> Context::parseTypeExpression() {
    return parsePrefix();
}

UniquePtr<ast::Expression> Context::parseComma() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Comma>(
        [this] { return parseAssignment(); });
}

UniquePtr<ast::Expression> Context::parseAssignment() {
    using enum ast::BinaryOperator;
    return parseBinaryOperatorRTL<Assignment, AddAssignment, SubAssignment,
                                  MulAssignment, DivAssignment, RemAssignment,
                                  LSAssignment, RSAssignment, AndAssignment,
                                  OrAssignment, XOrAssignment>(
        [this] { return parseConditional(); });
}

UniquePtr<ast::Expression> Context::parseConditional() {
    Token const& condToken = tokens.peek();
    auto logicalOr = parseLogicalOr();
    if (auto const& questionMark = tokens.peek();
        questionMark.kind() == Question)
    {
        if (!logicalOr) {
            pushExpectedExpression(condToken);
        }
        tokens.eat();
        auto lhs = parseComma();
        if (!lhs) {
            pushExpectedExpression(tokens.peek());
        }
        Token const& colon = tokens.peek();
        if (colon.kind() != Colon) {
            issues.push<UnqualifiedID>(colon, Colon);
        }
        else {
            tokens.eat();
        }
        auto rhs = parseConditional();
        if (!rhs) {
            pushExpectedExpression(tokens.peek());
        }
        return allocate<ast::Conditional>(std::move(logicalOr), std::move(lhs),
                                          std::move(rhs),
                                          questionMark.sourceRange());
    }
    return logicalOr;
}

UniquePtr<ast::Expression> Context::parseLogicalOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalOr>(
        [this] { return parseLogicalAnd(); });
}

UniquePtr<ast::Expression> Context::parseLogicalAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalAnd>(
        [this] { return parseInclusiveOr(); });
}

UniquePtr<ast::Expression> Context::parseInclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseOr>(
        [this] { return parseExclusiveOr(); });
}

UniquePtr<ast::Expression> Context::parseExclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseXOr>(
        [this] { return parseAnd(); });
}

UniquePtr<ast::Expression> Context::parseAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseAnd>(
        [this] { return parseEquality(); });
}

UniquePtr<ast::Expression> Context::parseEquality() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Equals,
                                  ast::BinaryOperator::NotEquals>(
        [this] { return parseRelational(); });
}

UniquePtr<ast::Expression> Context::parseRelational() {
    return parseBinaryOperatorLTR<
        ast::BinaryOperator::Less, ast::BinaryOperator::LessEq,
        ast::BinaryOperator::Greater, ast::BinaryOperator::GreaterEq>(
        [this] { return parseShift(); });
}

UniquePtr<ast::Expression> Context::parseShift() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LeftShift,
                                  ast::BinaryOperator::RightShift>(
        [this] { return parseAdditive(); });
}

UniquePtr<ast::Expression> Context::parseAdditive() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Addition,
                                  ast::BinaryOperator::Subtraction>(
        [this] { return parseMultiplicative(); });
}

UniquePtr<ast::Expression> Context::parseMultiplicative() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Multiplication,
                                  ast::BinaryOperator::Division,
                                  ast::BinaryOperator::Remainder>(
        [this] { return parseCast(); });
}

UniquePtr<ast::Expression> Context::parseCast() {
    Token const& lhsToken = tokens.peek();
    UniquePtr<ast::Expression> left = parsePrefix();
    while (true) {
        Token const token = tokens.peek();
        if (token.kind() != As) {
            return left;
        }
        tokens.eat();
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        Token const& rhsToken = tokens.peek();
        UniquePtr<ast::Expression> right = parseTypeExpression();
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        left = allocate<ast::CastExpr>(std::move(left), std::move(right),
                                       token.sourceRange());
    }
}

UniquePtr<ast::Expression> Context::parsePrefix() {
    if (auto deref = parsePostfix()) {
        return deref;
    }
    Token const& token = tokens.peek();
    auto parseArith = [&](ast::UnaryOperator operatorType) {
        Token const unaryToken = tokens.peek();
        auto unary = parsePrefix();
        if (!unary) {
            pushExpectedExpression(unaryToken);
        }
        return allocate<ast::UnaryExpression>(
            operatorType, ast::UnaryOperatorNotation::Prefix, std::move(unary),
            token.sourceRange());
    };
    auto parseRef = [&]<typename Expr>() {
        auto sr = token.sourceRange();
        auto qualSet = parseQualifierSet(sr);
        return allocate<Expr>(parsePrefix(), sr, qualSet.mut, qualSet.bindMode);
    };
    switch (token.kind()) {
    case Plus:
        tokens.eat();
        return parseArith(ast::UnaryOperator::Promotion);
    case Minus:
        tokens.eat();
        return parseArith(ast::UnaryOperator::Negation);
    case Tilde:
        tokens.eat();
        return parseArith(ast::UnaryOperator::BitwiseNot);
    case Exclam:
        tokens.eat();
        return parseArith(ast::UnaryOperator::LogicalNot);
    case Increment:
        tokens.eat();
        return parseArith(ast::UnaryOperator::Increment);
    case Decrement:
        tokens.eat();
        return parseArith(ast::UnaryOperator::Decrement);
    case Multiplies: {
        tokens.eat();
        return parseRef.operator()<ast::DereferenceExpression>();
    }
    case BitAnd:
        tokens.eat();
        return parseRef.operator()<ast::AddressOfExpression>();
    case Move: {
        tokens.eat();
        auto value = parsePrefix();
        if (!value) {
            issues.push<ExpectedExpression>(tokens.peek());
            return nullptr;
        }
        return allocate<ast::MoveExpr>(std::move(value), token.sourceRange());
    }
    case Unique: {
        tokens.eat();
        auto sr = token.sourceRange();
        auto mutQual = parseMut(sr);
        return allocate<ast::UniqueExpr>(parsePrefix(), mutQual, sr);
    }
    default:
        return nullptr;
    }
}

UniquePtr<ast::Expression> Context::parsePostfix() {
    auto expr = parseGeneric();
    if (!expr) {
        return nullptr;
    }
    while (true) {
        auto const& token = tokens.peek();
        switch (token.kind()) {
        case Increment:
            expr = parseUnaryPostfix(ast::UnaryOperator::Increment,
                                     std::move(expr));
            break;
        case Decrement:
            expr = parseUnaryPostfix(ast::UnaryOperator::Decrement,
                                     std::move(expr));
            break;
        case OpenBracket:
            expr = parseSubscript(std::move(expr));
            break;
        case OpenParan:
            expr = parseFunctionCall(std::move(expr));
            break;
        case Dot:
            expr = parseMemberAccess(std::move(expr));
            break;
        default:
            return expr;
        }
    }
}

static bool isGenericID(ast::Expression const* expr) {
    // clang-format off
    return SC_MATCH (*expr) {
        [](ast::Expression const&) { return false; },
        [](ast::Identifier const& expr) {
            /// Only generic ID for now
            return expr.value() == "reinterpret";
        },
        [](ast::MemberAccess const& expr) {
            return isGenericID(expr.member());
        },
    }; // clang-format on
}

UniquePtr<ast::Expression> Context::parseGeneric() {
    auto expr = parseFString();
    if (!expr) {
        return nullptr;
    }
    if (isGenericID(expr.get()) && tokens.peek().kind() == Less) {
        return parseFunctionCallLike<ast::GenericExpression>(std::move(expr),
                                                             Less, Greater,
                                                             [this] {
            return parseAdditive();
        });
    }
    return expr;
}

static UniquePtr<ast::Literal> allocateStringLit(Token const& token,
                                                 ast::LiteralKind kind) {
    return allocate<ast::Literal>(token.sourceRange(), kind, token.id());
}

UniquePtr<ast::Expression> Context::parseFString() {
    auto const& begin = tokens.peek();
    if (begin.kind() != FStringLiteralBegin) {
        return parsePrimary();
    }
    tokens.eat();
    utl::small_vector<UniquePtr<ast::Expression>> operands;
    operands.push_back(
        allocateStringLit(begin, ast::LiteralKind::FStringBegin));
    for (int i = 0;; ++i) {
        if (i % 2 == 0) {
            auto expr = parseAssignment();
            if (!expr) {
                pushExpectedExpression(tokens.peek());
            }
            else {
                operands.push_back(std::move(expr));
            }
            continue;
        }
        auto const& tok = tokens.peek();
        if (tok.kind() == FStringLiteralContinue) {
            tokens.eat();
            operands.push_back(
                allocateStringLit(tok, ast::LiteralKind::FStringContinue));
            continue;
        }
        if (tok.kind() == FStringLiteralEnd) {
            tokens.eat();
            operands.push_back(
                allocateStringLit(tok, ast::LiteralKind::FStringEnd));
            break;
        }
        issues.push<ExpectedFStringEnd>(tokens.peek());
        break;
    }
    return allocate<ast::FStringExpr>(std::move(operands));
}

UniquePtr<ast::Expression> Context::parsePrimary() {
    if (auto result = parseID()) {
        return result;
    }
    if (auto result = parseLiteral()) {
        return result;
    }
    auto const& token = tokens.peek();
    if (token.kind() == OpenParan) {
        tokens.eat();
        Token const commaToken = tokens.peek();
        UniquePtr<ast::Expression> comma = parseComma();
        if (!comma) {
            pushExpectedExpression(commaToken);
        }
        expectDelimiter(CloseParan);
        return comma;
    }
    if (token.kind() == OpenBracket) {
        SourceRange sourceRange = token.sourceRange();
        tokens.eat();
        bool first = true;
        utl::small_vector<UniquePtr<ast::Expression>> elems;
        Token next = tokens.peek();
        while (next.kind() != CloseBracket) {
            ssize_t tokenIndex = tokens.index();
            if (!first) {
                expectDelimiter(Comma);
            }
            first = false;
            auto expr = parseConditional();
            if (!expr) {
                pushExpectedExpression(next);
                if (tokenIndex == tokens.index()) {
                    tokens.eat();
                }
            }
            elems.push_back(std::move(expr));
            next = tokens.peek();
        }
        tokens.eat();
        return allocate<ast::ListExpression>(std::move(elems),
                                             merge(sourceRange,
                                                   next.sourceRange()));
    }
    return nullptr;
}

UniquePtr<ast::Identifier> Context::parseID() {
    Token const token = tokens.peek();
    if (!isID(token.kind())) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::Identifier>(token.sourceRange(), token.id());
}

UniquePtr<ast::Identifier> Context::parseExtID() {
    Token const token = tokens.peek();
    if (!isExtendedID(token.kind())) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::Identifier>(token.sourceRange(), token.id());
}

UniquePtr<ast::Literal> Context::parseLiteral() {
    Token const token = tokens.peek();
    switch (token.kind()) {
    case IntegerLiteral:
        tokens.eat();
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::Integer,
                                      token.toInteger(64));
    case True:
        [[fallthrough]];
    case False:
        tokens.eat();
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::Boolean,
                                      token.toBool());
    case FloatLiteral:
        tokens.eat();
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::FloatingPoint,
                                      token.toFloat(APFloatPrec::Double()));
    case Null:
        tokens.eat();
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::Null);
    case This:
        tokens.eat();
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::This, APInt());
    case StringLiteral:
        tokens.eat();
        return allocateStringLit(token, ast::LiteralKind::String);
    case CharLiteral:
        tokens.eat();
        SC_ASSERT(token.id().size() == 1, "Invalid char literal");
        return allocate<ast::Literal>(token.sourceRange(),
                                      ast::LiteralKind::Char,
                                      APInt(static_cast<uint64_t>(
                                                token.id()[0]),
                                            8));
    default:
        return nullptr;
    }
}

template <typename FunctionCallLike>
UniquePtr<FunctionCallLike> Context::parseFunctionCallLike(
    UniquePtr<ast::Expression> primary, TokenKind open, TokenKind close) {
    return parseFunctionCallLike<FunctionCallLike>(std::move(primary), open,
                                                   close, [this] {
        return parseAssignment();
    });
}

template <typename FunctionCallLike>
UniquePtr<FunctionCallLike> Context::parseFunctionCallLike(
    UniquePtr<ast::Expression> primary, TokenKind open, TokenKind close,
    auto parseCallback) {
    auto const& openToken = tokens.peek();
    SC_EXPECT(openToken.kind() == open);
    auto args =
        parseList<utl::small_vector<UniquePtr<ast::Expression>>>(open, close,
                                                                 Comma,
                                                                 parseCallback)
            .value(); // TODO: Handle error
    auto const& closeToken = tokens.current();
    return allocate<FunctionCallLike>(std::move(primary), std::move(args),
                                      merge(openToken.sourceRange(),
                                            closeToken.sourceRange()));
}

template <typename, typename List>
std::optional<List> Context::parseList(TokenKind open, TokenKind close,
                                       TokenKind delimiter,
                                       auto parseCallback) {
    auto const& openToken = tokens.peek();
    if (openToken.kind() != open) {
        return std::nullopt;
    }
    tokens.eat();
    List result;
    bool first = true;
    while (true) {
        Token const next = tokens.peek();
        if (next.kind() == close) {
            tokens.eat();
            return result;
        }
        if (!first) {
            expectDelimiter(delimiter);
        }
        first = false;
        auto elem = parseCallback();
        if (elem) {
            result.push_back(std::move(elem));
        }
        else {
            /// TODO: Maybe delegate error handling to the parse callback to
            /// avoid duplicate errors in some cases
            issues.push<ExpectedExpression>(tokens.peek());
            /// Without eating a token we may get stuck in an infinite loop,
            /// otherwise we may miss delimiters in case of syntax errors
            /// (especcially missing ')').
            if (Token const next = tokens.peek();
                next.kind() != close && next.kind() != delimiter)
            {
                tokens.eat();
            }
        }
    }
    return result;
}

UniquePtr<ast::UnaryExpression> Context::parseUnaryPostfix(
    ast::UnaryOperator op, UniquePtr<ast::Expression> primary) {
    auto token = tokens.eat();
    SC_EXPECT(token.kind() == Increment || token.kind() == Decrement);
    return allocate<ast::UnaryExpression>(op,
                                          ast::UnaryOperatorNotation::Postfix,
                                          std::move(primary),
                                          token.sourceRange());
}

UniquePtr<ast::CallLike> Context::parseSubscript(
    UniquePtr<ast::Expression> primary) {
    auto openToken = tokens.peek();
    if (openToken.kind() != TokenKind::OpenBracket) {
        return nullptr;
    }
    tokens.eat();
    auto first = parseAssignment();
    auto nextTok = tokens.peek();
    switch (nextTok.kind()) {
    case TokenKind::CloseBracket: {
        tokens.eat();
        return allocate<ast::Subscript>(std::move(primary),
                                        toSmallVector(std::move(first)),
                                        openToken.sourceRange());
    }
    case TokenKind::Colon: {
        tokens.eat();
        auto second = parseAssignment();
        expectDelimiter(TokenKind::CloseBracket);
        return allocate<ast::SubscriptSlice>(std::move(primary),
                                             std::move(first),
                                             std::move(second),
                                             openToken.sourceRange());
    }
    default:
        issues.push<ExpectedDelimiter>(nextTok);
        return nullptr;
    }
}

UniquePtr<ast::FunctionCall> Context::parseFunctionCall(
    UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::FunctionCall>(std::move(primary),
                                                    OpenParan, CloseParan);
}

UniquePtr<ast::Expression> Context::parseMemberAccess(
    UniquePtr<ast::Expression> left) {
    while (true) {
        auto const& token = tokens.peek();
        if (token.kind() != Dot) {
            return left;
        }
        tokens.eat();
        auto right = parseID();
        if (!right) {
            issues.push<ExpectedExpression>(tokens.peek());
        }
        left = allocate<ast::MemberAccess>(std::move(left), std::move(right),
                                           token.sourceRange());
        continue;
    }
}

sema::Mutability Context::parseMut(SourceRange& sr) {
    if (tokens.peek().kind() == Mutable) {
        sr = merge(sr, tokens.peek().sourceRange());
        tokens.eat();
        return sema::Mutability::Mutable;
    }
    return sema::Mutability::Const;
}

Qualifier Context::parseQualifier(SourceRange& sr) {
    switch (tokens.peek().kind()) {
    case Mutable:
        sr = merge(sr, tokens.peek().sourceRange());
        tokens.eat();
        return sema::Mutability::Mutable;
    case Dynamic:
        sr = merge(sr, tokens.peek().sourceRange());
        tokens.eat();
        return sema::PointerBindMode::Dynamic;
    default:
        return std::monostate();
    }
    return sema::Mutability::Const;
}

QualifierSet Context::parseQualifierSet(SourceRange sr) {
    QualifierSet set{
        .mut = sema::Mutability::Const,
        .bindMode = sema::PointerBindMode::Static,
    };
    // clang-format off
    while (std::visit(csp::overload{
        [&](sema::Mutability m) { set.mut = m; return true; },
        [&](sema::PointerBindMode mode) { set.bindMode = mode; return true; },
        [](auto) { return false; }
    }, parseQualifier(sr))) { sr = tokens.peek().sourceRange(); }
    // clang-format on
    return set;
}

ast::SpecifierList Context::parseSpecList() {
    using sema::AccessControl;
    struct ParseResult {
        std::variant<sema::AccessControl, std::string> spec;
        SourceRange sourceRange;
    };
    auto parse = [&]() -> std::optional<ParseResult> {
        auto sourceRange = tokens.peek().sourceRange();
        switch (tokens.peek().kind()) {
        case Private:
            tokens.eat();
            return ParseResult{ AccessControl::Private, sourceRange };
        case Internal:
            tokens.eat();
            return ParseResult{ AccessControl::Internal, sourceRange };
        case Public:
            tokens.eat();
            return ParseResult{ AccessControl::Public, sourceRange };
        case Extern: {
            tokens.eat();
            auto next = tokens.peek();
            sourceRange = merge(sourceRange, next.sourceRange());
            if (next.kind() != StringLiteral) {
                issues.push<ExpectedExpression>(next);
                return std::nullopt;
            }
            tokens.eat();
            return ParseResult{ next.id(), sourceRange };
        }
        default:
            return std::nullopt;
        }
    };
    ast::SpecifierList list{};
    while (true) {
        auto result = parse();
        if (!result) {
            break;
        }
        std::visit([&](auto spec) { list.set(spec, result->sourceRange); },
                   result->spec);
    }
    return list;
}

void Context::pushExpectedExpression(Token const& token) {
    issues.push<ExpectedExpression>(token);
}

static TokenKind toTokenKind(ast::BinaryOperator op) {
    switch (op) {
    case ast::BinaryOperator::Multiplication:
        return Multiplies;
    case ast::BinaryOperator::Division:
        return Divides;
    case ast::BinaryOperator::Remainder:
        return Remainder;
    case ast::BinaryOperator::Addition:
        return Plus;
    case ast::BinaryOperator::Subtraction:
        return Minus;
    case ast::BinaryOperator::LeftShift:
        return LeftShift;
    case ast::BinaryOperator::RightShift:
        return RightShift;
    case ast::BinaryOperator::Less:
        return Less;
    case ast::BinaryOperator::LessEq:
        return LessEqual;
    case ast::BinaryOperator::Greater:
        return Greater;
    case ast::BinaryOperator::GreaterEq:
        return GreaterEqual;
    case ast::BinaryOperator::Equals:
        return Equal;
    case ast::BinaryOperator::NotEquals:
        return Unequal;
    case ast::BinaryOperator::BitwiseAnd:
        return BitAnd;
    case ast::BinaryOperator::BitwiseXOr:
        return BitXOr;
    case ast::BinaryOperator::BitwiseOr:
        return BitOr;
    case ast::BinaryOperator::LogicalAnd:
        return LogicalAnd;
    case ast::BinaryOperator::LogicalOr:
        return LogicalOr;
    case ast::BinaryOperator::Assignment:
        return Assign;
    case ast::BinaryOperator::AddAssignment:
        return PlusAssign;
    case ast::BinaryOperator::SubAssignment:
        return MinusAssign;
    case ast::BinaryOperator::MulAssignment:
        return MultipliesAssign;
    case ast::BinaryOperator::DivAssignment:
        return DividesAssign;
    case ast::BinaryOperator::RemAssignment:
        return RemainderAssign;
    case ast::BinaryOperator::LSAssignment:
        return LeftShiftAssign;
    case ast::BinaryOperator::RSAssignment:
        return RightShiftAssign;
    case ast::BinaryOperator::AndAssignment:
        return AndAssign;
    case ast::BinaryOperator::OrAssignment:
        return OrAssign;
    case ast::BinaryOperator::XOrAssignment:
        return XOrAssign;
    case ast::BinaryOperator::Comma:
        return Comma;
    default:
        SC_UNREACHABLE();
    }
}

template <ast::BinaryOperator... Op>
UniquePtr<ast::Expression> Context::parseBinaryOperatorLTR(auto&& operand) {
    Token const& lhsToken = tokens.peek();
    UniquePtr<ast::Expression> left = operand();
    auto tryParse = [&](Token const token, ast::BinaryOperator op) {
        if (token.kind() != toTokenKind(op)) {
            return false;
        }
        tokens.eat();
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        Token const& rhsToken = tokens.peek();
        UniquePtr<ast::Expression> right = operand();
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        left = allocate<ast::BinaryExpression>(op, std::move(left),
                                               std::move(right),
                                               token.sourceRange());
        return true;
    };
    while (true) {
        Token const token = tokens.peek();
        if ((tryParse(token, Op) || ...)) {
            continue;
        }
        return left;
    }
}

template <ast::BinaryOperator... Op>
UniquePtr<ast::Expression> Context::parseBinaryOperatorRTL(
    auto&& parseOperand) {
    Token const& lhsToken = tokens.peek();
    if (lhsToken.kind() == EndOfFile) {
        return nullptr;
    }
    UniquePtr<ast::Expression> left = parseOperand();
    Token const operatorToken = tokens.peek();
    auto parse = [&](ast::BinaryOperator op) {
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        tokens.eat();
        Token const& rhsToken = tokens.peek();
        UniquePtr<ast::Expression> right =
            parseBinaryOperatorRTL<Op...>(parseOperand);
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        return allocate<ast::BinaryExpression>(op, std::move(left),
                                               std::move(right),
                                               operatorToken.sourceRange());
    };
    if (UniquePtr<ast::Expression> result = nullptr;
        ((operatorToken.kind() == toTokenKind(Op) &&
          ((result = parse(Op)), true)) ||
         ...))
    {
        return result;
    }
    return left;
}

bool Context::expectDelimiter(TokenKind delimiter) {
    auto next = tokens.peek();
    if (next.kind() == delimiter) {
        tokens.eat();
        return true;
    }
    issues.push<ExpectedDelimiter>(next);
    return false;
}
