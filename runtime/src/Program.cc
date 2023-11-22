#include "Runtime/Program.h"

using namespace scatha;

std::optional<size_t> Program::getAddress(std::string name) const {
    auto itr = _sym.find(name);
    if (itr != _sym.end()) {
        return itr->second;
    }
    return std::nullopt;
}
