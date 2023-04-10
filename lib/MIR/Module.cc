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

Constant* Module::constant(uint64_t value) {
    auto [itr, success] = constants.insert({ value, Constant(value) });
    return &itr->second;
}
