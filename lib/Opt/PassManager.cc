#include "Opt/PassManager.h"

#include <memory>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

using namespace scatha;
using namespace opt;

using PassType = PassManager::PassType;

namespace {

struct Impl {
    utl::hashmap<std::string, PassType> passes;

    std::function<bool(ir::Context&, ir::Function&)> getPass(
        std::string_view name) const {
        auto itr = passes.find(name);
        if (itr != passes.end()) {
            return itr->second;
        }
        return nullptr;
    }

    std::function<bool(ir::Context&, ir::Module&)> makePipeline(
        std::string_view passes) const {
        SC_UNIMPLEMENTED();
    }

    utl::vector<std::string> getPassNames() const {
        return passes |
               ranges::views::transform([](auto& p) { return p.first; }) |
               ranges::to<utl::vector>;
    }

    void registerPass(PassType pass, std::string name) {
        auto [itr, success] = passes.insert({ name, pass });
        SC_ASSERT(success, "Failed to register pass");
    }
};

} // namespace

static Impl& getImpl() {
    static auto impl = std::make_unique<Impl>();
    return *impl;
}

std::function<bool(ir::Context&, ir::Function&)> PassManager::getPass(
    std::string_view name) {
    return getImpl().getPass(name);
}

std::function<bool(ir::Context&, ir::Module&)> PassManager::makePipeline(
    std::string_view passes) {
    return getImpl().makePipeline(passes);
}

utl::vector<std::string> PassManager::passes() {
    return getImpl().getPassNames();
}

void opt::internal::registerPass(PassType pass, std::string name) {
    getImpl().registerPass(pass, std::move(name));
}
