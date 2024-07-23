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
    utl::hashmap<std::string, LocalPass> localPasses;

    utl::hashmap<std::string, GlobalPass> globalPasses;

    auto getPassImpl(auto& map, std::string_view name) const {
        auto itr = map.find(name);
        if (itr != map.end()) {
            return itr->second;
        }
        return decltype(itr->second){};
    }

    LocalPass getPass(std::string_view name) const {
        if (auto pass = getPassImpl(localPasses, name)) {
            return pass;
        }
        return {};
    }

    GlobalPass getGlobalPass(std::string_view name) const {
        return getPassImpl(globalPasses, name);
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

    utl::vector<LocalPass> getLocalPasses(auto filter) const {
        return getPassesImpl(localPasses, filter);
    }

    void registerLocal(LocalPass pass) {
        auto [itr, success] =
            localPasses.insert({ pass.name(), std::move(pass) });
        SC_ASSERT(success, "Failed to register pass");
    }

    void registerGlobal(GlobalPass pass) {
        auto [itr, success] =
            globalPasses.insert({ pass.name(), std::move(pass) });
        SC_ASSERT(success, "Failed to register pass");
    }
};

} // namespace

static Impl& getImpl() {
    static auto impl = std::make_unique<Impl>();
    return *impl;
}

LocalPass PassManager::getPass(std::string_view name) {
    return getImpl().getPass(name);
}

GlobalPass PassManager::getGlobalPass(std::string_view name) {
    return getImpl().getGlobalPass(name);
}

Pipeline PassManager::makePipeline(std::string_view passes) {
    return getImpl().makePipeline(passes);
}

utl::vector<LocalPass> PassManager::localPasses() {
    return getImpl().getLocalPasses([](auto&) { return true; });
}

utl::vector<LocalPass> PassManager::localPasses(PassCategory category) {
    return getImpl().getLocalPasses(
        [=](auto& pass) { return pass.category() == category; });
}

void ir::internal::registerLocal(LocalPass pass) {
    getImpl().registerLocal(std::move(pass));
}

void ir::internal::registerGlobal(GlobalPass pass) {
    getImpl().registerGlobal(std::move(pass));
}
