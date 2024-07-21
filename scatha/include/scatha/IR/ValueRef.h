#ifndef SCATHA_IR_VALUEREF_H_
#define SCATHA_IR_VALUEREF_H_

#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Wraps a `Value*`
/// Instances are tracked by `Value` and set if null if the value is destroyed
class ValueRef {
public:
    ValueRef() = default;
    ValueRef(Value* value);
    ValueRef(ValueRef const&);
    ValueRef& operator=(ValueRef const&);
    ~ValueRef();

    /// \Returns the wrapped value pointer
    Value* value() const { return _value; }

    /// Sets the pointer to null
    void reset();

private:
    friend class Value;

    Value* _value = nullptr;
};

} // namespace scatha::ir

#endif // SCATHA_IR_VALUEREF_H_
