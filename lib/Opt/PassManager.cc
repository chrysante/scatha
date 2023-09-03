#include "Opt/PassManager.h"
#include "Opt/PassRegistry.h"

#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Opt/Pipeline/PipelineParser.h"

using namespace scatha;
using namespace opt;

namespace {

struct Impl {
    utl::hashmap<std::string, LocalPass> localPasses;

    utl::hashmap<std::string, LocalPass> canonicalizationPasses;

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
        if (auto pass = getPassImpl(canonicalizationPasses, name)) {
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
                                    auto filter) const {
        return map | ranges::views::values | ranges::views::filter(filter) |
               ranges::to<utl::vector>;
    }

    utl::vector<LocalPass> getLocalPasses() const {
        return getPassesImpl(localPasses, [](auto const& pass) {
            return pass.name() != "default";
        });
    }

    utl::vector<LocalPass> getCanonicalizationPasses() const {
        return getPassesImpl(localPasses, [](auto const& pass) {
            return pass.name() != "canonicalize";
        });
    }

    void registerLocal(LocalPass pass) {
        auto [itr, success] = localPasses.insert({ pass.name(), pass });
        SC_ASSERT(success, "Failed to register pass");
    }

    void registerCanonicalization(LocalPass pass) {
        auto [itr, success] =
            canonicalizationPasses.insert({ pass.name(), pass });
        SC_ASSERT(success, "Failed to register pass");
    }

    void registerGlobal(GlobalPass pass) {
        auto [itr, success] = globalPasses.insert({ pass.name(), pass });
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
    return getImpl().getLocalPasses();
}

void opt::internal::registerLocal(LocalPass pass) {
    getImpl().registerLocal(std::move(pass));
}

void opt::internal::registerCanonicalization(LocalPass pass) {
    getImpl().registerCanonicalization(std::move(pass));
}

void opt::internal::registerGlobal(GlobalPass pass) {
    getImpl().registerGlobal(std::move(pass));
}
