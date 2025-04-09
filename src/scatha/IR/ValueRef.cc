#include "IR/ValueRef.h"

#include "IR/CFG/Value.h"

using namespace scatha;
using namespace ir;

ValueRef::ValueRef(Value* value): _value(value) {
    value->_references.insert(this);
}

ValueRef::ValueRef(ValueRef const& rhs): ValueRef(rhs.value()) {}

ValueRef& ValueRef::operator=(ValueRef const& rhs) {
    if (value() == rhs.value()) {
        return *this;
    }
    reset();
    _value = rhs.value();
    _value->_references.insert(this);
    return *this;
}

ValueRef::~ValueRef() { reset(); }

void ValueRef::reset() {
    if (_value) {
        _value->_references.erase(this);
    }
}
