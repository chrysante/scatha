#ifndef SCATHA_IR_CFG_CONSTANT_H_
#define SCATHA_IR_CFG_CONSTANT_H_

#include "Basic/Basic.h"
#include "IR/CFG/User.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Represents a (global) constant value.
class SCATHA_API Constant: public User {
public:
    using User::User;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANT_H_
