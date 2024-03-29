#ifndef SCATHA_IR_FOREACH_H_
#define SCATHA_IR_FOREACH_H_

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/IR/Pass.h>

namespace scatha::ir {

/// Execute \p localPass for each function in the module \p mod
SCATHA_API bool forEach(Context& ctx, Module& mod, LocalPass localPass);

} // namespace scatha::ir

#endif // SCATHA_IR_FOREACH_H_
