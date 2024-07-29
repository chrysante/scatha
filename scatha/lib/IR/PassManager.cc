#include "IR/PassManager.h"
#include "IR/PassRegistry.h"

#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "IR/PipelineParser.h"

using namespace scatha;
using namespace ir;
using namespace ranges::views;

namespace {

struct Impl {
    utl::hashmap<std::string, FunctionPass> functionPasses;

    utl::hashmap<std::string, ModulePass> modulePasses;

    auto getPassImpl(auto& map, std::string_view name) const {
        auto itr = map.find(name);
        if (itr != map.end()) {
            return itr->second;
        }
        return decltype(itr->second){};
    }

    FunctionPass getFunctionPass(std::string_view name) const {
        if (auto pass = getPassImpl(functionPasses, name)) {
            return pass;
        }
        return {};
    }

    ModulePass getModulePass(std::string_view name) const {
        return getPassImpl(modulePasses, name);
    }

    Pipeline makePipeline(std::string_view text) const {
        return parsePipeline(text);
    }

    template <typename Pass>
    utl::vector<Pass> getPassesImpl(utl::hashmap<std::string, Pass> const& map,
                                    auto cond) const {
#ifndef _MSC_VER
        return map | values | filter(cond) | ranges::to<utl::vector>;
#else
        utl::vector<Pass> result;
        for (auto& [name, pass]: map) {
            if (filter(pass)) {
                result.push_back(pass);
            }
        }
        return result;
#endif
    }

    utl::vector<FunctionPass> getFunctionPasses(auto filter) const {
        return getPassesImpl(functionPasses, filter);
    }

    void registerFunctionPass(FunctionPass pass) {
        auto [itr, success] =
            functionPasses.insert({ pass.name(), std::move(pass) });
        SC_ASSERT(success, "Failed to register pass");
    }

    void registerModulePass(ModulePass pass) {
        auto [itr, success] =
            modulePasses.insert({ pass.name(), std::move(pass) });
        SC_ASSERT(success, "Failed to register pass");
    }
};

} // namespace

static Impl& getImpl() {
    static auto impl = std::make_unique<Impl>();
    return *impl;
}

FunctionPass PassManager::getFunctionPass(std::string_view name) {
    return getImpl().getFunctionPass(name);
}

ModulePass PassManager::getModulePass(std::string_view name) {
    return getImpl().getModulePass(name);
}

Pipeline PassManager::makePipeline(std::string_view passes) {
    return getImpl().makePipeline(passes);
}

utl::vector<FunctionPass> PassManager::functionPasses() {
    return getImpl().getFunctionPasses([](auto&) { return true; });
}

utl::vector<FunctionPass> PassManager::functionPasses(PassCategory category) {
    return getImpl().getFunctionPasses(
        [=](auto& pass) { return pass.category() == category; });
}

void ir::internal::registerFunctionPass(FunctionPass pass) {
    getImpl().registerFunctionPass(std::move(pass));
}

void ir::internal::registerModulePass(ModulePass pass) {
    getImpl().registerModulePass(std::move(pass));
}
