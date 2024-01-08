#include "Common/FFI.h"

#include <algorithm>

using namespace scatha;

ForeignFunctionInterface::ForeignFunctionInterface(
    std::string name,
    std::span<svm::FFIType const> argTypes,
    svm::FFIType retType):
    _name(std::move(name)) {
    sig.push_back(retType);
    std::copy(argTypes.begin(), argTypes.end(), std::back_inserter(sig));
}
