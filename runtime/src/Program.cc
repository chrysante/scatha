#include "Runtime/Program.h"

using namespace scatha;

std::optional<size_t> Program::getAddress(std::string name) const {
    auto itr = _binsym.find(name);
    if (itr != _binsym.end()) {
        return itr->second;
    }
    return std::nullopt;
}
