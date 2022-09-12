#pragma once

#ifndef SCATHA_BASIC_MEMORY_H_
#define SCATHA_BASIC_MEMORY_H_

#include <type_traits>
#include <cstring>

namespace scatha {
	
	template <typename T>
	T read(void const* ptr) {
		std::aligned_storage_t<sizeof(T), alignof(T)> storage;
		std::memcpy(&storage, ptr, sizeof(T));
		return reinterpret_cast<T const&>(storage);
	}

	template <typename T>
	void store(void* dest, T const& t) {
		std::memcpy(dest, &t, sizeof(T));
	}
	
}

#endif // SCATHA_BASIC_MEMORY_H_
