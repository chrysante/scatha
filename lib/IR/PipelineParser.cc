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
    enum Type { Identifier, Separator, OpenParan, CloseParan, End };

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
    case Token::Separator:
        return ",";
    case Token::OpenParan:
        return "(";
    case Token::CloseParan:
        return ")";
    case Token::End:
        return "<end>";
    }
}

std::ostream& operator<<(std::ostream& str, Token::Type type) {
    return str << toString(type);
}

class Lexer {
public:
    Lexer(std::string_view text): text(text) {}

    Token next() {
        while (index < text.size() && std::isspace(text[index])) {
            ++column;
            if (text[index] == '\n') {
                ++line;
                column = 0;
            }
            ++index;
        }
        if (index == text.size()) {
            return Token(Token::End, {}, line, column);
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
        if (isIDBegin(text[index])) {
            size_t begin = index;
            size_t count = 1;
            ++index;
            while (index < text.size() && isIDContinue(text[index])) {
                ++index;
                ++count;
            }
            return Token(Token::Identifier, text.substr(begin, count), line,
                         column);
        }
        throw PipelineLexicalError(line + 1, column + 1, "Invalid token");
    }

private:
    static bool isIDBegin(char c) { return std::isalpha(c); }

    static bool isIDContinue(char c) { return std::isalnum(c) || c == '_'; }

    std::optional<Token> getSingleCharToken(char c, Token::Type type) {
        if (text[index] == c) {
            return Token(type, text.substr(index++, 1), line, column);
        }
        return std::nullopt;
    }

    std::string_view text;
    size_t index = 0;
    size_t column = 0;
    size_t line = 0;
};

class Parser {
public:
    Parser(std::string_view text): lex(text), peekToken(lex.next()) {}

    Pipeline parse() {
        auto root = std::make_unique<PipelineRoot>(parseGlobalList());
        return Pipeline(std::move(root));
    }

    utl::small_vector<std::unique_ptr<PipelineGlobalNode>> parseGlobalList() {
        return parseList<PipelineGlobalNode>([this] { return parseGlobal(); },
                                             "global node");
    }

    std::unique_ptr<PipelineGlobalNode> parseGlobal() {
        auto token = peek();
        if (token.type != Token::Identifier) {
            return nullptr;
        }
        auto globalPass = PassManager::getGlobalPass(token.id);
        if (!globalPass) {
            return parseImplicitForeach();
        }
        eat();
        if (peek().type != Token::OpenParan) {
            return std::make_unique<PipelineGlobalNode>(std::move(globalPass));
        }
        eat();
        auto localList = parseLocalList();
        expect(Token::CloseParan);
        return std::make_unique<PipelineGlobalNode>(std::move(globalPass),
                                                    std::move(localList));
    }

    std::unique_ptr<PipelineGlobalNode> parseImplicitForeach() {
        auto localNode = parseLocal();
        if (!localNode) {
            return nullptr;
        }
        return std::make_unique<PipelineGlobalNode>(PassManager::getGlobalPass(
                                                        "foreach"),
                                                    std::move(localNode));
    }

    utl::small_vector<std::unique_ptr<PipelineLocalNode>> parseLocalList() {
        return parseList<PipelineLocalNode>([this] { return parseLocal(); },
                                            "local node");
    }

    template <typename T>
    utl::small_vector<std::unique_ptr<T>> parseList(auto parseCB,
                                                    std::string_view type) {
        utl::small_vector<std::unique_ptr<T>> result;
        if (auto node = parseCB()) {
            result.push_back(std::move(node));
        }
        else {
            return result;
        }
        while (true) {
            auto token = peek();
            if (token.type != Token::Separator) {
                return result;
            }
            eat();
            auto node = parseCB();
            if (!node) {
                throw makeError(peek(), utl::strcat("Expected ", type));
            }
            result.push_back(std::move(node));
        }
        return result;
    }

    std::unique_ptr<PipelineLocalNode> parseLocal() {
        auto token = peek();
        if (token.type != Token::Identifier) {
            return nullptr;
        }
        auto pass = PassManager::getPass(token.id);
        if (!pass) {
            return nullptr;
        }
        eat();
        return std::make_unique<PipelineLocalNode>(std::move(pass));
    }

    Token peek() { return peekToken; }

    Token eat() {
        auto result = peekToken;
        peekToken = lex.next();
        return result;
    }

    void expect(Token::Type type) {
        auto token = eat();
        if (token.type != type) {
            throw makeError(token, utl::strcat("Expected '", type, "'"));
        }
    }

    static PipelineSyntaxError makeError(Token const& token,
                                         std::string_view message) {
        return PipelineSyntaxError(token.line, token.column, message);
    }

private:
    Lexer lex;
    Token peekToken;
};

} // namespace

Pipeline ir::parsePipeline(std::string_view text) {
    return Parser(text).parse();
}
