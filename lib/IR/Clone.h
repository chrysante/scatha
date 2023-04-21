#ifndef SCATHA_IR_CLONE_H_
#define SCATHA_IR_CLONE_H_

#include "Basic/Basic.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class Context;

SCATHA_TESTAPI
UniquePtr<Function> clone(Context& context, Function* function);

} // namespace scatha::ir

#endif // SCATHA_IR_CLONE_H_
