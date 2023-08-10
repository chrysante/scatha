#ifndef SCATHA_OPT_PASSMANAGERIMPL_H_
#define SCATHA_OPT_PASSMANAGERIMPL_H_

#include <string_view>

#include "Common/Base.h"
#include "Opt/Pipeline.h"

/// # Pipeline script grammar
///
///
/// ```
///
/// pipeline         => global-pass-list
/// global-pass-list => global-pass [ ( ":" global-pass )* ]
/// global-pass      => for-each-pass
///                   | inline-pass
///                   | dfe-pass
/// for-each-pass    => "foreach" "(" local-pass-list ")"
///                   | local-pass-id
/// inline-pass      => "inline" [ "(" local-pass-list ")" ]
/// dfe-pass         => "deadfuncelim"
/// local-pass-list  => local-pass-id [ ( ":" local-pass-id )* ]
///
/// ```

namespace scatha::opt {

/// Parse a transform pipeline from \p script
SCATHA_API Pipeline parsePipeline(std::string_view script);

} // namespace scatha::opt

#endif // SCATHA_OPT_PASSMANAGERIMPL_H_
