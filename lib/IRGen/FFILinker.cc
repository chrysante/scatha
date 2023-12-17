#include "IRGen/FFILinker.h"

#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/dynamic_library.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace irgen;
using namespace ir;
using namespace ranges::views;

static utl::hashmap<std::string_view, size_t> makeBuiltinIndexMap() {
    size_t index = 0;
    return {
#define SVM_BUILTIN_DEF(name, ...)                                             \
    std::pair{ std::string_view("__builtin_" #name), index++ },
#include <svm/Builtin.def>
    };
}

Expected<void, FFILinkError> irgen::linkFFIs(
    ir::Module& mod, std::span<std::filesystem::path const> libs) {

    auto FFs = mod.extFunctions() |
               ranges::to<utl::hashset<ir::ForeignFunction*>>;
    for (auto itr = FFs.begin(); itr != FFs.end();) {
        auto* F = *itr;
        if (!F->name().starts_with("__builtin_")) {
            ++itr;
            continue;
        }
        static utl::hashmap<std::string_view, size_t> const builtinIndexMap =
            makeBuiltinIndexMap();
        if (auto indexItr = builtinIndexMap.find(F->name());
            indexItr != builtinIndexMap.end())
        {
            F->setAddress(uint16_t(~0),
                          svm::BuiltinFunctionSlot,
                          indexItr->second);
            itr = FFs.erase(itr);
            continue;
        }
        ++itr;
    }
    static constexpr size_t FFSlot = 2;
    size_t FFIndex = 0;
    for (auto [libIndex, path]: libs | enumerate) {
        utl::dynamic_library lib(path);
        if (!lib.is_open()) {
            continue;
        }
        for (auto itr = FFs.begin(); itr != FFs.end();) {
            try {
                auto* F = *itr;
                (void)lib.symbol_ptr<void()>(utl::strcat("sc_ffi_", F->name()));
                F->setAddress(libIndex, FFSlot, FFIndex++);
                FFs.erase(itr);
            }
            catch (std::exception const&) {
                ++itr;
            }
        }
    }
    if (!FFs.empty()) {
        return FFILinkError{ .missingFunctions =
                                 FFs | transform([](auto* F) {
                                     return std::string(F->name());
                                 }) |
                                 ranges::to<std::vector> };
    }
    return {};
}
