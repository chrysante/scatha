#ifndef SVM_MEMORY_H_
#define SVM_MEMORY_H_

#include <cstring>
#include <type_traits>

namespace svm {

template <typename T>
T load(void const* ptr) {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::memcpy(&storage, ptr, sizeof(T));
    return reinterpret_cast<T const&>(storage);
}

template <typename T>
void store(void* dest, T const& t) {
    std::memcpy(dest, &t, sizeof(T));
}

} // namespace svm

#endif // SVM_MEMORY_H_
