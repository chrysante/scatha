#include "MIR/Module.h"

#include <utl/utility.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

Module::Module() = default;

Module::Module(Module&&) noexcept = default;

Module& Module::operator=(Module&&) noexcept = default;

Module::~Module() = default;

void Module::addFunction(Function* function) { funcs.push_back(function); }

void Module::addGlobal(Value* value) {
    _globals.insert(UniquePtr<Value>(value));
    if (auto* F = dyncast<ForeignFunction*>(value)) {
        foreignFuncsNEW.insert(F);
    }
}

std::pair<void*, size_t> Module::allocateStaticData(std::string name,
                                                    size_t size, size_t align) {
    size_t offset = utl::round_up(staticData.size(), align);
    staticData.resize(offset + size);
    _dataNames.insert({ offset, std::move(name) });
    return { staticData.data() + offset, offset };
}

void Module::addAddressPlaceholder(size_t offset, Function const* function) {
    addrPlaceholders.push_back({ offset, function });
}
