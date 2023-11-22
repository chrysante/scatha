#include "Runtime/Runtime.h"

using namespace scatha;

bool Runtime::declareFunction(std::string name,
                              sema::FunctionSignature signature,
                              InternalFuncPtr impl,
                              void* userptr) {
    auto decl = comp.declareFunction(std::move(name), std::move(signature));
    if (decl) {
        exec.addFunction(decl, impl, userptr);
        return true;
    }
    return false;
}
