#ifndef SCATHA_IR_PARAMETER_H_
#define SCATHA_IR_PARAMETER_H_

#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class Function;

class Parameter: public Value, public NodeWithParent<Parameter, Function> {
    using NodeBase = NodeWithParent<Parameter, Function>;

public:
    explicit Parameter(Type const* type, std::string name, Function* parent): Value(NodeType::Parameter, type, std::move(name)), NodeBase(parent) {}

private:
};

} // namespace scatha::ir

#endif // SCATHA_IR_PARAMETER_H_
