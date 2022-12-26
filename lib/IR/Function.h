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
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name);
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string_view name):
        Function(functionType, returnType, parameterTypes, std::string(name)) {}

    Function(Function const&) = delete;

    ~Function();

    Type const* returnType() const { return _returnType; }

    List<Parameter>& parameters() { return params; }
    List<Parameter> const& parameters() const { return params; }

    List<BasicBlock>& basicBlocks() { return bbs; }
    List<BasicBlock> const& basicBlocks() const { return bbs; }

    void addBasicBlock(BasicBlock* basicBlock);

private:
    Type const* _returnType;
    List<Parameter> params;
    List<BasicBlock> bbs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_FUNCTION_H_
