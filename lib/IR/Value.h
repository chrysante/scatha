#ifndef SCATHA_IR_VALUE_H_
#define SCATHA_IR_VALUE_H_

#include <string>

#include "IR/Type.h"

namespace scatha::ir {
	
/// Represents a value in the program.
/// Every value has a type. Types are not values.
class Value {
public:
    explicit Value(Type const* type):
        _name(), _type(type) {}
    
    explicit Value(std::string name, Type const* type):
        _name(std::move(name)), _type(type) {}
    
    Type const* type() const { return _type; }
    
    std::string_view name() const { return _name; }
    
    bool hasName() const { return !_name.empty(); }
    
    void setName(std::string name) {
        _name = std::move(name);
    }
    
private:
    std::string _name;
    Type const* _type;
};

class Constant: public Value {
public:
    using Value::Value;
    
private:
    
};

} // namespace scatha::ir

#endif // SCATHA_IR_VALUE_H_

