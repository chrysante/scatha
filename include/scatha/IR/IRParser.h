#ifndef SCATHA_IR_IRPARSER_H_
#define SCATHA_IR_IRPARSER_H_

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

/// Communication channel for parser callbacks
class DeclToken {
public:
    /// Called by a parser callback to ignore a declaration.
    /// \Warning Currently this may only be called on type callbacks
    void ignore() { _shallIgnore = true; }

    ///
    bool shallIgnore() const { return _shallIgnore; }

private:
    bool _shallIgnore = false;
};

/// Options structure for `parseTo()`
struct ParseOptions {
    /// Callback that will be invoked when a type is parsed
    utl::function_view<void(ir::StructType&, DeclToken&)> typeParseCallback;

    /// Callback that will be invoked when a global is parsed
    utl::function_view<void(ir::Global&, DeclToken&)> objectParseCallback;

    /// Assert invariants after parsing
    bool assertInvariants = true;
};

/// Parses \p text into the IR module \p mod
/// \Returns a vector of issues encountered. If said vector is empty the
/// function succeeded
SCATHA_API std::vector<ParseIssue> parseTo(std::string_view text, Context& ctx,
                                           Module& mod,
                                           ParseOptions const& options = {});

} // namespace scatha::ir

#endif // SCATHA_IR_IRPARSER_H_
