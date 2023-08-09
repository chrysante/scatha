#include "Opt/PassManager.h"

#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Opt/Pipeline/PipelineParser.h"

using namespace scatha;
using namespace opt;

using PassType = PassManager::PassType;

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
        return getPassImpl(localPasses, name);
    }

    GlobalPass getGlobalPass(std::string_view name) const {
        return getPassImpl(globalPasses, name);
    }

    Pipeline makePipeline(std::string_view text) const {
        return parsePipeline(text);
    }

    void registerPass(LocalPass pass) {
        auto [itr, success] = localPasses.insert({ pass.name(), pass });
        SC_ASSERT(success, "Failed to register pass");
    }

    void registerPass(GlobalPass pass) {
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

void opt::internal::registerPass(LocalPass pass) {
    getImpl().registerPass(std::move(pass));
}

void opt::internal::registerPass(GlobalPass pass) {
    getImpl().registerPass(std::move(pass));
}
