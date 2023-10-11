#ifndef SCATHA_IR_CFG_CONSTANT_H_
#define SCATHA_IR_CFG_CONSTANT_H_

#include "Common/Base.h"
#include "IR/CFG/User.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a constant value.
class SCATHA_API Constant: public User {
public:
    /// Writes the value of this constant to the memory at \p dest
    /// Caller is responsible that dest buffer is sufficiently large
    void writeValueTo(void* dest) const;

protected:
    using User::User;

private:
    void writeValueToImpl(void* dest) const { SC_UNREACHABLE(); }
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANT_H_
