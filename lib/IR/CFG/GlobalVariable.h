#ifndef SCATHA_IR_CFG_GLOBALVARIABLE_H_
#define SCATHA_IR_CFG_GLOBALVARIABLE_H_

#include "IR/CFG/Global.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a (mutable) global variable
class SCATHA_API GlobalVariable: public Global {
public:
    enum Mutability { Mutable, Const };

    ///
    GlobalVariable(Context& ctx, Mutability mutability, Constant* initializer,
                   std::string name);

    /// The constant that initializes this value. Can be `undef` for dynamic
    /// initialization
    Constant* initializer() {
        return const_cast<Constant*>(std::as_const(*this).initializer());
    }

    /// \overload
    Constant const* initializer() const;

    /// Setter for two step initialization (the parser needs this)
    void setInitializer(Constant* init);

    /// Global variables are always of type `ptr`
    PointerType const* type() const;

    /// Mutability of this variable
    Mutability mutability() const { return mut; }

    /// Shorthand for `mutability() == Mutable`
    bool isMutable() const { return mutability() == Mutable; }

    /// Shorthand for `!isMutable()`
    bool isConst() const { return !isMutable(); }

private:
    Mutability mut;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBALVARIABLE_H_
