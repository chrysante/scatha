#ifndef SCATHA_IR_MODULE_H_
#define SCATHA_IR_MODULE_H_

#include "Basic/Basic.h"
#include "IR/List.h"
#include "IR/CFGCommon.h"

namespace scatha::ir {

class SCATHA(API) Module {
public:
    Module() = default;
    Module(Module const& rhs) = delete;
    Module(Module&& rhs) noexcept;
    Module& operator=(Module const& rhs) = delete;
    Module& operator=(Module&& rhs) noexcept;
    ~Module();

    List<Function> const& functions() const { return funcs; }

    void addFunction(Function* function);

private:
    List<Function> funcs;
};

} // namespace scatha::ir

#endif // SCATHA_IR_MODULE_H_
