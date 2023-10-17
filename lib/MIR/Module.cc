#include "MIR/Module.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

Module::Module(): undef(new UndefValue()) {
    undef->set_next(undef.get());
    undef->set_prev(undef.get());
}

Module::Module(Module&&) noexcept = default;

Module& Module::operator=(Module&&) noexcept = default;

Module::~Module() = default;

void Module::addFunction(Function* function) { funcs.push_back(function); }

Constant* Module::constant(uint64_t value, size_t width) {
    auto [itr, success] =
        constants.insert({ { value, width }, Constant(value, width) });
    return &itr->second;
}

std::pair<void*, size_t> Module::allocateStaticData(size_t size) {
    size_t offset = staticData.size();
    staticData.resize(staticData.size() + size);
    return { staticData.data() + offset, offset };
}

void Module::addAddressPlaceholder(size_t offset, Function const* function) {
    addrPlaceholders.push_back({ offset, function });
}
