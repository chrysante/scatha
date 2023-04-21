#ifndef SCATHA_PARSE_PARANCOUNTER_H_
#define SCATHA_PARSE_PARANCOUNTER_H_

#include <string_view>

#include "AST/Token.h"
#include "Common/Base.h"

namespace scatha::parse {

class ParanCounter {
public:
    void count(Token const& token) {
        countImpl(token.id, _parans, "(", ")");
        countImpl(token.id, _brackets, "[", "]");
        countImpl(token.id, _braces, "{", "}");
    }
    ssize_t parans() const { return _parans; }
    ssize_t brackets() const { return _brackets; }
    ssize_t braces() const { return _braces; }

private:
    void countImpl(std::string_view id,
                   ssize_t& counter,
                   std::string_view open,
                   std::string_view close) {
        if (id == open) {
            ++counter;
        }
        if (id == close) {
            --counter;
        }
    }

private:
    ssize_t _parans   = 0;
    ssize_t _brackets = 0;
    ssize_t _braces   = 0;
};

} // namespace scatha::parse

#endif // SCATHA_PARSE_PARANCOUNTER_H_
