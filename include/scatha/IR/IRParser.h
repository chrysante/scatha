#ifndef SCATHA_IR_PARSER_H_
#define SCATHA_IR_PARSER_H_

#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/IR/Parser/IRIssue.h>
#include <scatha/IR/Parser/IRSourceLocation.h>

namespace scatha::ir {

class Context;
class Module;

/// Parses \p text into an IR module.
SCATHA_API Expected<std::pair<Context, Module>, ParseIssue> parse(
    std::string_view text);

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_H_
