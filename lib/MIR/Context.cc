#include "MIR/Context.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

Context::Context(): _undef(new UndefValue()) {
    _undef->set_next(undef());
    _undef->set_prev(undef());
}

Context::Context(Context&&) noexcept = default;

Context& Context::operator=(Context&&) noexcept = default;

Context::~Context() = default;

Constant* Context::constant(uint64_t value, size_t bytewidth) {
    auto [itr, success] =
        constants.insert({ { value, bytewidth }, Constant(value, bytewidth) });
    return &itr->second;
}
