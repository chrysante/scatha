#include "IR/Module.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

void Module::addFunction(Function* function) {
    function->set_parent(this);
    funcs.push_back(function);
}

Module::Module(Module&& rhs) noexcept = default;

Module& Module::operator=(Module&& rhs) noexcept = default;

Module::~Module() = default;
