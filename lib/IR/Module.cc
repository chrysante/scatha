#include "IR/Module.h"

using namespace scatha;

using namespace ir;

void Module::addFunction(Function* function) {
    function->set_parent(this);
    funcs.push_back(function);
}
