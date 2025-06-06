#ifndef SCATHA_IR_CFG_GLOBAL_H_
#define SCATHA_IR_CFG_GLOBAL_H_

#include <scatha/Common/List.h>
#include <scatha/IR/CFG/Constant.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Represents a global value
class SCATHA_API Global:
    public Constant,
    public ListNode<Global>,
    public ParentedNode<Module> {
public:
    /// \Returns The visibility of this global in the binary symbol table
    Visibility visibility() const { return vis; }

    /// Sets the visibility to \p vis
    void setVisibility(Visibility vis) { this->vis = vis; }

protected:
    explicit Global(NodeType nodeType, Type const* type, std::string name = {},
                    std::span<Value* const> operands = {},
                    Visibility vis = Visibility::Internal):
        Constant(nodeType, type, std::move(name), operands), vis(vis) {}

private:
    Visibility vis;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_GLOBAL_H_
