#include "MIR/Context.h"

#include <utl/functional.hpp>

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

Constant* Context::constant(APInt value) {
    SC_EXPECT(value.bitwidth() <= 64);
    return constant(value.to<uint64_t>(),
                    utl::ceil_divide(value.bitwidth(), 8));
}
