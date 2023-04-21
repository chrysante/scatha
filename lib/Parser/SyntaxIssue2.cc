#include "Parser/SyntaxIssue2.h"

#include <ostream>

#include <utl/utility.hpp>

#include "Issue/IssueHandler2.h"

using namespace scatha;
using namespace parse;

void SyntaxError::doPrint(std::ostream&) const { SC_DEBUGFAIL(); }

std::string SyntaxError::doToString() const { SC_DEBUGFAIL(); }
