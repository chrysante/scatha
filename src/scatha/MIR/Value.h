#ifndef SCATHA_MIR_VALUE_H_
#define SCATHA_MIR_VALUE_H_

#include "Common/List.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Abstract base class of all values in the MIR
class Value: public ListNode<Value, /* AllowSetSiblings = */ true> {
public:
    /// \Returns The most derived run time type of this value
    NodeType nodeType() const { return _nodeType; }

protected:
    Value(NodeType nodeType): _nodeType(nodeType) {}

private:
    friend NodeType get_rtti(Value const& value) { return value.nodeType(); }

    NodeType _nodeType;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_VALUE_H_
