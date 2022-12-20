#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_


#include "Basic/Basic.h"
#include "IR/Function.h"
#include "IR/List.h"

namespace scatha::ir {

class Module {
public:
    List<Function> const& functions() const { return funcs; }
    
    void addFunction(Function* function);
    
private:
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_

