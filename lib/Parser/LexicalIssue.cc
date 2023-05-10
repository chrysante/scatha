#include "Parser/LexicalIssue.h"

#include <utl/strcat.hpp>

using namespace scatha;
using namespace parse;

std::string InvalidEscapeSequence::message() const {
    return utl::strcat("Invalid escape sequence: ", '\\', litValue);
}
