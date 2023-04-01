#pragma once

#ifndef SCATHA_BASIC_MEMORY_H_
#define SCATHA_BASIC_MEMORY_H_

#include <cstring>
#include <type_traits>

namespace scatha {

template <typename T>
T load(void const* ptr) {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::memcpy(&storage, ptr, sizeof(T));
    return reinterpret_cast<T const&>(storage);
}

template <typename T>
[[deprecated("Use `load` instead")]] T read(void const* ptr) {
    return load<T>(ptr);
}

template <typename T>
void store(void* dest, T const& t) {
    std::memcpy(dest, &t, sizeof(T));
}

} // namespace scatha

#endif // SCATHA_BASIC_MEMORY_H_
