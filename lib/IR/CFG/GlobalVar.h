#ifndef SCATHA_IR_CFG_GLOBALVAR_H_
#define SCATHA_IR_CFG_GLOBALVAR_H_

#include "IR/CFG/Global.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Common base class of `GlobalVar` and `GlobalConst`
class SCATHA_API GlobalVarBase: public Global {
public:
    /// The constant that initializes this value. Can be `undef` for dynamic
    /// initialization
    Constant* initializer() {
        return const_cast<Constant*>(std::as_const(*this).initializer());
    }

    /// \overload
    Constant const* initializer() const;

    /// Global variables are always of type `ptr`
    PointerType const* type() const;

protected:
    GlobalVarBase(NodeType nodeType,
                  ir::Context& ctx,
                  Constant* initializer,
                  std::string name);

private:
    Constant* init;
};

/// Represents a (mutable) global variable
class SCATHA_API GlobalVar: public GlobalVarBase {
public:
    GlobalVar(ir::Context& ctx, Constant* initializer, std::string name);
};

/// Represents a constant global variable
class SCATHA_API GlobalConst: public GlobalVarBase {
public:
    GlobalConst(ir::Context& ctx, Constant* initializer, std::string name);
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBALVAR_H_
