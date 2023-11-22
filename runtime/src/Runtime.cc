#include "Runtime/Runtime.h"

using namespace scatha;

Runtime::Runtime(): exec(Executor::Make()) {}

bool Runtime::addFunction(std::string name,
                          sema::FunctionSignature signature,
                          InternalFuncPtr impl,
                          void* userptr) {
    auto decl = comp.declareFunction(std::move(name), std::move(signature));
    if (decl) {
        exec->addFunction(decl, impl, userptr);
        return true;
    }
    return false;
}

bool Runtime::compile() {
    try {
        exec->load(comp.compile());
        return true;
    }
    catch (std::runtime_error const&) {
        return false;
    }
}
