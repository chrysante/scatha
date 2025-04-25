#ifndef SCATHA_UTIL_SET_H_
#define SCATHA_UTIL_SET_H_

#include <initializer_list>
#include <iterator>

namespace test {

template <typename T>
struct Set {
    Set(std::initializer_list<T> l): elems(l) {}

    utl::small_vector<T> elems;
};

template <typename T>
bool operator==(Set<T> const& s, auto const& rng) {
    if (std::size(rng) != s.elems.size()) {
        return false;
    }
    for (auto&& x: rng) {
        for (auto&& y: s.elems) {
            if (x == y) {
                goto endLoop;
            }
        }
        return false;
endLoop:
        continue;
    }
    return true;
}

} // namespace test

#endif // SCATHA_UTIL_SET_H_
