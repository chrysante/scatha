#ifndef SCATHA_IR_PASSMANAGERIMPL_H_
#define SCATHA_IR_PASSMANAGERIMPL_H_

#include <string_view>

#include "Common/Base.h"
#include "IR/Pipeline.h"

/// Pipeline script grammar
///
///     pipeline         => global-pass-list
///
///     global-pass-list => global-pass
///                       | global-pass "," global-pass-list
///
///     global-pass      => global-pass-id
///                       | global-pass-id "(" local-pass-list ")"
///                       | local-pass-id
///
///     global-pass-id   => global-pass-name
///                       | global-pass-name "[" arg-list "]"
///
///     local-pass-list  => local-pass-id
///                       | local-pass-id "," local-pass-list
///
///     local-pass-id    => local-pass-name
///                       | local-pass-name "[" arg-list "]"
///
///     arg-list         => arg
///                       | arg "," arg-list
///
///     arg              => id | id ":" value
///

namespace scatha::ir {

/// Parse a transform pipeline from \p script
SCATHA_API Pipeline parsePipeline(std::string_view script);

} // namespace scatha::ir

#endif // SCATHA_IR_PASSMANAGERIMPL_H_
