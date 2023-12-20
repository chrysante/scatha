#include "Common/ForeignFunctionDecl.h"

#include <utl/hashtable.hpp>

using namespace scatha;

static utl::hashmap<std::string_view, size_t> makeBuiltinIndexMap() {
    size_t index = 0;
    return {
#define SVM_BUILTIN_DEF(name, ...)                                             \
    std::pair{ std::string_view("__builtin_" #name), index++ },
#include <svm/Builtin.def>
    };
}

std::optional<size_t> scatha::getBuiltinIndex(std::string_view name) {
    static auto const builtinIndexMap = makeBuiltinIndexMap();
    if (auto itr = builtinIndexMap.find(name); itr != builtinIndexMap.end()) {
        return itr->second;
    }
    return std::nullopt;
}
