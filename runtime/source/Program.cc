#include <scatha/Runtime/Program.h>

#include "CommonImpl.h"

using namespace scatha;

std::optional<size_t> Program::findAddress(
    std::string_view name, std::span<QualType const> argTypes) const {
    auto mangledName = mangleFunctionName(name, argTypes);
    auto itr         = sym.find(mangledName);
    if (itr == sym.end()) {
        return std::nullopt;
    }
    return itr->second;
}
