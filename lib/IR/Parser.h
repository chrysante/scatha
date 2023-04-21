#ifndef SCATHA_IR_PARSER_H_
#define SCATHA_IR_PARSER_H_

#include <string_view>

#include "Common/Base.h"
#include "Common/Expected.h"
#include "IR/Parser/Issue.h"
#include "IR/Parser/SourceLocation.h"

namespace scatha::ir {

class Context;
class Module;

/// Parses \p text into an IR module.
SCATHA_API Expected<std::pair<Context, Module>, ParseIssue> parse(
    std::string_view text);

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_H_
