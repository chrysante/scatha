#ifndef SCATHA_IR_PASSMANAGERIMPL_H_
#define SCATHA_IR_PASSMANAGERIMPL_H_

#include <string_view>

#include "Common/Base.h"
#include "IR/Pipeline.h"

/// Pipeline script grammar
///
///     pipeline            => module-pass-list
///
///     module-pass-list    => module-pass
///                          | module-pass "," module-pass-list
///
///     module-pass         => module-pass-id
///                          | module-pass-id "(" function-pass-list ")"
///                          | function-pass
///
///     module-pass-id      => module-pass-name
///                          | module-pass-name "[" arg-list "]"
///
///     function-pass-list  => function-pass
///                          | function-pass "," function-pass-list
///
///     function-pass       => function-pass-id
///                          | "loop" "(" loop-pass-list ")"
///
///     function-pass-id    => function-pass-name
///                          | function-pass-name "[" arg-list "]"
///
///     loop-pass-list      => loop-pass
///                          | loop-pass "," loop-pass-list
///
///     loop-pass           => loop-pass-id
///                          | loop-pass-id "[" arg-list "]"
///
///     arg-list            => arg
///                          | arg "," arg-list
///
///     arg                 => id | id ":" value
///
///     value               => id | string-lit | numeric-lit
///

namespace scatha::ir {

/// Parse a transform pipeline from \p script
SCATHA_API Pipeline parsePipeline(std::string_view script);

} // namespace scatha::ir

#endif // SCATHA_IR_PASSMANAGERIMPL_H_
