#ifndef SCATHA_IR_CFG_GLOBALVAR_H_
#define SCATHA_IR_CFG_GLOBALVAR_H_

#include "IR/CFG/Global.h"

namespace scatha::ir {

/// Represents a global value
class SCATHA_API GlobalVar: public Global {
protected:
    using Global::Global;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBALVAR_H_
