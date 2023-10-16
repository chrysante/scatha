#ifndef SCATHA_IR_CFG_GLOBAL_H_
#define SCATHA_IR_CFG_GLOBAL_H_

#include "IR/CFG/Constant.h"

namespace scatha::ir {

/// Represents a global value
class SCATHA_API Global: public Constant {
protected:
    using Constant::Constant;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBAL_H_
