#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Label.h"
#include "IR/VariableDeclaration.h"
#include "IR/Function.h"
#include "IR/BranchStatement.h"

namespace scatha::ir {

class Module {
public:
    explicit Module(utl::vector<Function> funcs):
        funcs(std::move(funcs))
    {}
    
    std::span<Function const> functions() const { return funcs; }
    
private:
    utl::vector<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_

