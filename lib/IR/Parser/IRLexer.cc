#include "IR/Parser/IRLexer.h"

#include <array>
#include <cctype>
#include <optional>
#include <string_view>

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Common/Base.h"
#include "IR/Parser/IRToken.h"

using namespace scatha;
using namespace ir;

bool isPunctuation(char c) {
    static constexpr std::array punctuations = { '(', ')', '[', ']', '{',
                                                 '}', '=', ':', ',' };
    return ranges::find(punctuations, c) != ranges::end(punctuations);
};

static std::optional<TokenKind> getPunctuation(char c) {
    switch (c) {
    case '(':
        return TokenKind::OpenParan;
    case ')':
        return TokenKind::CloseParan;
    case '{':
        return TokenKind::OpenBrace;
    case '}':
        return TokenKind::CloseBrace;
    case '[':
        return TokenKind::OpenBracket;
    case ']':
        return TokenKind::CloseBracket;
    case '=':
        return TokenKind::Assign;
    case ',':
        return TokenKind::Comma;
    case ':':
        return TokenKind::Colon;
    default:
        return std::nullopt;
    }
}

static std::optional<TokenKind> getKeyword(std::string_view id) {
    static constexpr std::pair<std::string_view, TokenKind> values[] = {
        { "struct", TokenKind::Structure },
        { "func", TokenKind::Function },
        { "global", TokenKind::Global },
        { "constant", TokenKind::Constant },
        { "void", TokenKind::Void },
        { "ptr", TokenKind::Ptr },
        { "null", TokenKind::NullLiteral },
        { "undef", TokenKind::UndefLiteral },
        { "alloca", TokenKind::Alloca },
        { "load", TokenKind::Load },
        { "store", TokenKind::Store },

#define SC_CONVERSION_DEF(Op, Keyword) { #Keyword, TokenKind::Op },
#include "IR/Lists.def"

        { "goto", TokenKind::Goto },
        { "branch", TokenKind::Branch },
        { "return", TokenKind::Return },
        { "call", TokenKind::Call },
        { "phi", TokenKind::Phi },
        { "scmp", TokenKind::SCmp },
        { "ucmp", TokenKind::UCmp },
        { "fcmp", TokenKind::FCmp },
        { "bnt", TokenKind::Bnt },
        { "lnt", TokenKind::Lnt },
        { "neg", TokenKind::Neg },
        { "add", TokenKind::Add },
        { "sub", TokenKind::Sub },
        { "mul", TokenKind::Mul },
        { "sdiv", TokenKind::SDiv },
        { "udiv", TokenKind::UDiv },
        { "srem", TokenKind::SRem },
        { "urem", TokenKind::URem },
        { "fadd", TokenKind::FAdd },
        { "fsub", TokenKind::FSub },
        { "fmul", TokenKind::FMul },
        { "fdiv", TokenKind::FDiv },
        { "lshl", TokenKind::LShL },
        { "lshr", TokenKind::LShR },
        { "ashl", TokenKind::AShL },
        { "ashr", TokenKind::AShR },
        { "and", TokenKind::And },
        { "or", TokenKind::Or },
        { "xor", TokenKind::XOr },
        { "getelementptr", TokenKind::GetElementPointer },
        { "insert_value", TokenKind::InsertValue },
        { "extract_value", TokenKind::ExtractValue },
        { "select", TokenKind::Select },
        { "ext", TokenKind::Ext },
        { "to", TokenKind::To },
        { "label", TokenKind::Label },
        { "inbounds", TokenKind::Inbounds },
        { "eq", TokenKind::Equal },
        { "neq", TokenKind::NotEqual },
        { "ls", TokenKind::Less },
        { "leq", TokenKind::LessEq },
        { "grt", TokenKind::Greater },
        { "geq", TokenKind::GreaterEq },
    };
    auto itr = ranges::find_if(values, [&](auto& p) { return p.first == id; });
    if (itr == ranges::end(values)) {
        return std::nullopt;
    }
    return itr->second;
}

Expected<Token, LexicalIssue> Lexer::next() {
    while (i != end && std::isspace(*i)) {
        inc();
    }
    // Comments
    if (*i == '#') {
        while (i != end && *i != '\n') {
            inc();
        }
        return next();
    }
    // End of file
    if (i == end) {
        return Token(std::string_view{}, loc, TokenKind::EndOfFile);
    }
    // Identifier
    if (*i == '@' || *i == '%') {
        SourceLocation const beginSL = loc;
        char const* const first = i;
        while (i != end && !std::isspace(*i) && !isPunctuation(*i)) {
            inc();
        }
        TokenKind const kind = *first == '@' ? TokenKind::GlobalIdentifier :
                                               TokenKind::LocalIdentifier;
        return Token(first + 1, i, beginSL, kind);
    }
    // NumericLiteral
    if (std::isdigit(*i) || *i == '-') {
        SourceLocation const beginSL = loc;
        char const* const first = i;
        size_t numDots = 0;
        if (*i == '-') {
            inc();
        }
        while (i != end && !std::isspace(*i) && !isPunctuation(*i)) {
            if (*i == '.') {
                ++numDots;
                inc();
                if (numDots > 1) {
                    return LexicalIssue(beginSL);
                }
                continue;
            }
            else if (std::isdigit(*i)) {
                inc();
                continue;
            }
            return LexicalIssue(beginSL);
        }

        if (numDots == 0) {
            return Token(first, i, beginSL, TokenKind::IntLiteral);
        }
        else {
            SC_ASSERT(numDots == 1, "");
            return Token(first, i, beginSL, TokenKind::FloatLiteral);
        }
    }
    // String literal
    if (*i == '\"') {
        SourceLocation const beginSL = loc;
        inc();
        auto* begin = i;
        while (i != end && *i != '\"') {
            inc();
        }
        auto* end = i;
        inc();
        return Token(begin, end, beginSL, TokenKind::StringLiteral);
    }
    // Punctuation
    if (auto kind = getPunctuation(*i)) {
        SourceLocation const beginSL = loc;
        auto first = i;
        inc();
        return Token(first, i, beginSL, *kind);
    }
    // Keyword
    if (std::isalpha(*i) || *i == '_') {
        SourceLocation const beginSL = loc;
        char const* const first = i;
        do {
            inc();
        } while (i != end && (std::isalnum(*i) || *i == '_'));
        std::string_view const id(first, static_cast<size_t>(i - first));
        auto keyword = getKeyword(id);
        if (keyword) {
            return Token(id, beginSL, *keyword);
        }
        if ((*first == 'i' || *first == 'f') &&
            ranges::all_of(first + 1, i, [](char c) {
            return std::isdigit(c);
        }))
        {
            char* strEnd = nullptr;
            long const width = std::strtol(first + 1, &strEnd, 10);
            SC_ASSERT(first + 1 != strEnd, "Failed to parse width");
            auto const kind = *first == 'i' ? TokenKind::IntType :
                                              TokenKind::FloatType;
            return Token(id, beginSL, kind, utl::narrow_cast<unsigned>(width));
        }
        return LexicalIssue(beginSL);
    }
    return LexicalIssue(loc);
}

void Lexer::inc() {
    if (i == end) {
        return;
    }
    ++i;
    ++loc.column;
    if (i[-1] != '\n') {
        return;
    }
    loc.column = 0;
    ++loc.line;
}
