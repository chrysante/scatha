#include "IR/PipelineParser.h"

#include <cctype>
#include <optional>
#include <stdexcept>

#include <utl/strcat.hpp>

#include "IR/PassManager.h"
#include "IR/PipelineError.h"
#include "IR/PipelineNodes.h"

using namespace scatha;
using namespace ir;

namespace {

struct Token {
    enum Type {
        Identifier,
        NumericLiteral,
        Separator,
        OpenParan,
        CloseParan,
        OpenBracket,
        CloseBracket,
        Colon,
        StringLit,
        End
    };

    Token(Type type, std::string_view id, size_t column, size_t line):
        type(type), id(id), column(column), line(line) {}

    Type type;
    std::string_view id;
    size_t column, line;
};

static std::string toString(Token::Type type) {
    switch (type) {
    case Token::Identifier:
        return "id";
    case Token::NumericLiteral:
        return "<num>";
    case Token::Separator:
        return ",";
    case Token::OpenParan:
        return "(";
    case Token::CloseParan:
        return ")";
    case Token::OpenBracket:
        return "[";
    case Token::CloseBracket:
        return "]";
    case Token::Colon:
        return ":";
    case Token::StringLit:
        return "<string-lit>";
    case Token::End:
        return "<end>";
    }
}

static std::ostream& operator<<(std::ostream& str, Token::Type type) {
    return str << toString(type);
}

class Lexer {
public:
    Lexer(std::string_view text): text(text) {}

    Token next() {
        while (index() < text.size() && std::isspace(text[index()])) {
            inc();
        }
        if (index() == text.size()) {
            return Token(Token::End, {}, line(), column());
        }
        if (auto token = getSingleCharToken(',', Token::Separator)) {
            return *token;
        }
        if (auto token = getSingleCharToken('(', Token::OpenParan)) {
            return *token;
        }
        if (auto token = getSingleCharToken(')', Token::CloseParan)) {
            return *token;
        }
        if (auto token = getSingleCharToken('[', Token::OpenBracket)) {
            return *token;
        }
        if (auto token = getSingleCharToken(']', Token::CloseBracket)) {
            return *token;
        }
        if (auto token = getSingleCharToken(':', Token::Colon)) {
            return *token;
        }
        if (isNumeric(text[index()])) {
            size_t begin = index();
            inc();
            while (index() < text.size() && isNumeric(text[index()])) {
                inc();
            }
            return Token(Token::NumericLiteral,
                         text.substr(begin, index() - begin), line(), column());
        }
        if (isIDBegin(text[index()])) {
            size_t begin = index();
            inc();
            while (index() < text.size() && isIDContinue(text[index()])) {
                inc();
            }
            return Token(Token::Identifier, text.substr(begin, index() - begin),
                         line(), column());
        }
        if (text[index()] == '\'' || text[index()] == '\"') {
            char delim = text[index()];
            inc();
            size_t begin = index();
            while (text[index()] != delim) {
                inc();
                if (index() == text.size() || text[index()] == '\n') {
                    throw PipelineLexicalError(line() + 1, column() + 1,
                                               "Unterminated string literal");
                }
            }
            Token tok(Token::StringLit, text.substr(begin, index() - begin),
                      line(), column());
            inc();
            return tok;
        }
        throw PipelineLexicalError(line() + 1, column() + 1, "Invalid token");
    }

private:
    static bool isIDBegin(char c) { return std::isalpha(c) || c == '_'; }

    static bool isIDContinue(char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    }

    static bool isNumeric(char c) { return std::isdigit(c); }

    std::optional<Token> getSingleCharToken(char c, Token::Type type) {
        if (text[index()] == c) {
            Token tok(type, text.substr(index(), 1), line(), column());
            inc();
            return tok;
        }
        return std::nullopt;
    }

    void inc() {
        if (index() >= text.size()) {
            return;
        }
        ++_column;
        if (text[index()] == '\n') {
            _column = 0;
            ++_line;
        }
        ++_index;
    }

    size_t index() const { return _index; }
    size_t column() const { return _column; }
    size_t line() const { return _line; }

    std::string_view text;
    size_t _index = 0;
    size_t _column = 0;
    size_t _line = 0;
};

class Parser {
public:
    Parser(std::string_view text): _lex(text), _peekToken(_lex.next()) {}

    Pipeline parse() {
        auto root = std::make_unique<PipelineRoot>(parseModulePassList());
        return Pipeline(std::move(root));
    }

    utl::small_vector<std::unique_ptr<PipelineModuleNode>>
        parseModulePassList() {
        return parseList<PipelineModuleNode>([this] {
            return parseModulePass();
        }, "global node");
    }

    std::unique_ptr<PipelineModuleNode> parseModulePass() {
        auto token = peek();
        if (token.type != Token::Identifier) {
            return nullptr;
        }
        auto modulePass = PassManager::getModulePass(token.id);
        if (!modulePass) {
            return parseImplicitForeach();
        }
        eat();
        parseArguments(modulePass);
        if (peek().type != Token::OpenParan) {
            return std::make_unique<PipelineModuleNode>(std::move(modulePass));
        }
        eat();
        auto fnPassList = parseFunctionPassList();
        expect(Token::CloseParan);
        return std::make_unique<PipelineModuleNode>(std::move(modulePass),
                                                    std::move(fnPassList));
    }

    std::unique_ptr<PipelineModuleNode> parseImplicitForeach() {
        auto fnNode = parseFunctionPass();
        if (!fnNode) {
            return nullptr;
        }
        return std::make_unique<PipelineModuleNode>(PassManager::getModulePass(
                                                        "foreach"),
                                                    std::move(fnNode));
    }

    utl::small_vector<std::unique_ptr<PipelineFunctionNode>>
        parseFunctionPassList() {
        return parseList<PipelineFunctionNode>([this] {
            return parseFunctionPass();
        }, "local node");
    }

    void parseList(auto parseCB, std::string_view type) {
        if (!parseCB()) {
            return;
        }
        while (true) {
            auto token = peek();
            if (token.type != Token::Separator) {
                return;
            }
            eat();
            if (!parseCB()) {
                throw makeError(peek(), utl::strcat("Expected ", type));
            }
        }
    }

    template <typename T>
    utl::small_vector<std::unique_ptr<T>> parseList(auto parseCB,
                                                    std::string_view type) {
        utl::small_vector<std::unique_ptr<T>> result;
        parseList([&] {
            if (auto node = parseCB()) {
                result.push_back(std::move(node));
                return true;
            }
            return false;
        }, type);
        return result;
    }

    std::unique_ptr<PipelineFunctionNode> parseFunctionPass() {
        auto token = peek();
        if (token.type != Token::Identifier) {
            return nullptr;
        }
        auto pass = PassManager::getFunctionPass(token.id);
        if (!pass) {
            return nullptr;
        }
        eat();
        parseArguments(pass);
        return std::make_unique<PipelineFunctionNode>(std::move(pass));
    }

    void parseArguments(PassBase& pass) {
        if (peek().type != Token::OpenBracket) {
            return;
        }
        eat();
        parseList([&] { return parseArgument(pass); }, "argument");
        expect(Token::CloseBracket);
    }

    void matchArg(PassBase& pass, Token key, std::string_view value) {
        using enum ArgumentMatchResult;
        switch (pass.matchArgument(key.id, value)) {
        case Success:
            return;
        case UnknownArgument:
            throw makeError(key, "Unknown argument '", key.id, "'");
        case BadValue:
            throw makeError(key, "Bad value for argument '", key.id, "'");
        }
        SC_UNREACHABLE();
    }

    bool parseArgument(PassBase& pass) {
        if (peek().type != Token::Identifier) {
            return false;
        }
        auto name = eat();
        if (peek().type != Token::Colon) {
            /// Flags are true when specified
            matchArg(pass, name, "YES");
            return true;
        }
        eat(); // Colon
        matchArg(pass, name, parseValue());
        return true;
    }

    std::string parseValue() {
        auto tok = eat();
        switch (tok.type) {
        case Token::Identifier:
            [[fallthrough]];
        case Token::NumericLiteral:
            [[fallthrough]];
        case Token::StringLit:
            return std::string(tok.id);
        default:
            throw makeError(tok, "Unexpected value token");
        }
    }

    Token peek() { return _peekToken; }

    Token eat() {
        auto result = _peekToken;
        _peekToken = _lex.next();
        return result;
    }

    void expect(Token const& token, Token::Type type) {
        if (token.type != type) {
            throw makeError(token, utl::strcat("Expected '", type, "'"));
        }
    }

    void expect(Token::Type type) { expect(eat(), type); }

    static PipelineSyntaxError makeError(Token const& token,
                                         std::string_view message) {
        return PipelineSyntaxError(token.line, token.column, message);
    }

    static PipelineSyntaxError makeError(Token const& token,
                                         auto&&... message) {
        return PipelineSyntaxError(token.line, token.column,
                                   utl::strcat(message...));
    }

private:
    Lexer _lex;
    Token _peekToken;
};

} // namespace

Pipeline ir::parsePipeline(std::string_view text) {
    return Parser(text).parse();
}
