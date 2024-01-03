#ifndef SCATHA_IR_PARSER_H_
#define SCATHA_IR_PARSER_H_

#include <string_view>
#include <vector>

#include <utl/function_view.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Expected.h>
#include <scatha/IR/Fwd.h>
#include <scatha/IR/Parser/IRIssue.h>
#include <scatha/IR/Parser/IRSourceLocation.h>

namespace scatha::ir {

class Context;
class Module;

/// Parses \p text into a new IR module
SCATHA_API Expected<std::pair<Context, Module>, ParseIssue> parse(
    std::string_view text);

/// Options structure for `parseTo()`
struct ParseOptions {
    /// Callback that will be invoked when a type is parsed
    utl::function_view<void(ir::StructType&)> typeParseCallback;

    /// Callback that will be invoked when a global is parsed
    utl::function_view<void(ir::Global&)> objectParseCallback;
};

/// Parses \p text into the IR module \p mod
/// \Returns a vector of issues encountered. If said vector is empty the
/// function succeeded
SCATHA_API std::vector<ParseIssue> parseTo(std::string_view text,
                                           Context& ctx,
                                           Module& mod,
                                           ParseOptions const& options = {});

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_H_
