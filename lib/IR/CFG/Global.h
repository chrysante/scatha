#ifndef SCATHA_IR_CFG_GLOBAL_H_
#define SCATHA_IR_CFG_GLOBAL_H_

#include "Common/List.h"
#include "IR/CFG/Constant.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a global value
class SCATHA_API Global:
    public Constant,
    public ListNode<Function>,
    public ParentedNode<Module> {
protected:
    using Constant::Constant;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBAL_H_
