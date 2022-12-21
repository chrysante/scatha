#ifndef SCATHA_IR_VALUE_H_
#define SCATHA_IR_VALUE_H_

#include <string>

#include "IR/CFGCommon.h"
#include "IR/Type.h"

namespace scatha::ir {

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class Value {
protected:
    explicit Value(NodeType nodeType, Type const* type): _nodeType(nodeType), _type(type) {}

    explicit Value(NodeType nodeType, Type const* type, std::string name) noexcept:
        _nodeType(nodeType), _type(type), _name(std::move(name)) {}

public:
    NodeType nodeType() const { return _nodeType; }

    Type const* type() const { return _type; }

    std::string_view name() const { return _name; }

    bool hasName() const { return !_name.empty(); }

    void setName(std::string name) { _name = std::move(name); }

private:
    NodeType _nodeType;
    Type const* _type;
    std::string _name;
};

// For dyncast compatibilty
NodeType dyncastGetType(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

class Constant: public Value {
public:
    using Value::Value;

private:
};

} // namespace scatha::ir

#endif // SCATHA_IR_VALUE_H_
