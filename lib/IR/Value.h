#ifndef SCATHA_IR_VALUE_H_
#define SCATHA_IR_VALUE_H_

#include <string>

#include "Common/APInt.h"
#include "IR/CFGCommon.h"
#include "IR/Type.h"

namespace scatha::ir {

class Context;

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA(API) Value {
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
inline NodeType dyncast_get_type(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

class SCATHA(API) Constant: public Value {
public:
    using Value::Value;

private:
};

class SCATHA(API) IntegralConstant: public Constant {
public:
    explicit IntegralConstant(Context& context, APInt value, size_t bitWidth);

    APInt const& value() const { return _value; }

private:
    APInt _value;
};

} // namespace scatha::ir

#endif // SCATHA_IR_VALUE_H_
