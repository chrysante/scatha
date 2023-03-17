#ifndef SCATHA_BASIC_OPAQUERANGE_H_
#define SCATHA_BASIC_OPAQUERANGE_H_

#include <range/v3/view.hpp>

namespace scatha {

auto makeOpaqueRange(auto&& r) {
    auto pass = []<typename T>(T&& value) -> decltype(auto) {
        return std::forward<T>(value);
    };
    return r | ranges::views::transform(pass);
}

} // namespace scatha

#endif // SCATHA_BASIC_OPAQUERANGE_H_
