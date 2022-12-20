#ifndef SCATHA_IR_FUNCTION_H_
#define SCATHA_IR_FUNCTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/BasicBlock.h"
#include "IR/List.h"
#include "IR/Parameter.h"
#include "IR/Type.h"
#include "IR/Value.h"

namespace scatha::ir {

class Module;

class Function: public Constant, public NodeWithParent<Function, Module> {
public:
    explicit Function(FunctionType* functionType, std::span<Type const*> parameterTypes):
        Constant(NodeType::Function, functionType) {
        for (auto* type: parameterTypes) {
            params.push_back(new Parameter(type, this));
        }
    }

    List<Parameter> const& parameters() const { return params; }

    List<BasicBlock> const& basicBlocks() const { return bbs; }

    void addBasicBlock(BasicBlock* bb) { bbs.push_back(bb); }

private:
    List<Parameter> params;
    List<BasicBlock> bbs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_FUNCTION_H_
