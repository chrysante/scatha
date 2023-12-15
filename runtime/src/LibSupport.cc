#include "Runtime/LibSupport.h"

#include <map>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include <svm/VirtualMachine.h>

using namespace scatha;
using namespace ranges::views;

std::vector<std::function<internal::DeclPair()>> internal::globalLibDecls;

std::vector<std::function<internal::DefPair()>> internal::globalLibDefines;

extern "C" void internal_declareFunctions(scatha::DeclareCallback callback,
                                          void* context) {
    using namespace internal;
    auto decls = globalLibDecls | transform([&](auto& f) { return f(); }) |
                 ranges::to<std::vector>;
    ranges::sort(
        decls,
        [](auto* s, auto* t) { return strcmp(s, t) < 0; },
        &DeclPair::first);
    for (auto& [name, sig]: decls) {
        callback(context, name, std::move(sig));
    }
}

extern "C" void internal_defineFunctions(scatha::DefineCallback callback,
                                         void* context) {
    using namespace internal;
    auto defs = globalLibDefines | transform([&](auto& f) { return f(); }) |
                ranges::to<std::vector>;
    ranges::sort(
        defs,
        [](auto* s, auto* t) { return strcmp(s, t) < 0; },
        &DefPair::first);

    size_t index = 0;
    for (auto [name, impl]: defs) {
        callback(context, index++, name, impl, nullptr);
    }
}
