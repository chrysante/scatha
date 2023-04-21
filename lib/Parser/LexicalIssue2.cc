#include "Parser/LexicalIssue2.h"

using namespace scatha;
using namespace parse;

void LexicalError::doPrint(std::ostream&) const { SC_DEBUGFAIL(); }

std::string LexicalError::doToString() const { SC_DEBUGFAIL(); }
