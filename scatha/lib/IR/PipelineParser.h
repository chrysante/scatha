#ifndef SCATHA_IR_PASSMANAGERIMPL_H_
#define SCATHA_IR_PASSMANAGERIMPL_H_

#include <string_view>

#include "Common/Base.h"
#include "IR/Pipeline.h"

/// # Pipeline script grammar
///
/// ```
/// pipeline         => global-pass-list
/// global-pass-list => global-pass [ ( "," global-pass )* ]
///
/// global-pass      => global-pass-id [ "(" local-pass-list ")" ]
///                   | local-pass-id
/// local-pass-list  => local-pass-id [ ( "," local-pass-id )* ]
/// ```

namespace scatha::ir {

/// Parse a transform pipeline from \p script
SCATHA_API Pipeline parsePipeline(std::string_view script);

} // namespace scatha::ir

#endif // SCATHA_IR_PASSMANAGERIMPL_H_
