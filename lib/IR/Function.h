#ifndef SCATHA_IR_FUNCTION_H_
#define SCATHA_IR_FUNCTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/List.h"
#include "IR/Value.h"

namespace scatha::ir {

class Module;
class Parameter;
class BasicBlock;
class Type;

class SCATHA(API) Function: public Constant, public NodeWithParent<Function, Module> {
public:
    explicit Function(FunctionType* functionType, std::span<Type const*> parameterTypes);
    
    Function(Function const&) = delete;
    
    ~Function();

    List<Parameter> const& parameters() const { return params; }

    List<BasicBlock> const& basicBlocks() const { return bbs; }

    void addBasicBlock(BasicBlock* basicBlock);

private:
    List<Parameter> params;
    List<BasicBlock> bbs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_FUNCTION_H_
