#ifndef SCATHA_COMMON_ELEMENTTABLE_H_
#define SCATHA_COMMON_ELEMENTTABLE_H_

#include <memory>

#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/strcat.hpp>

#include "Basic/Basic.h"

namespace scatha {
	
	template <typename T>
	class ElementTable {
	public:
		T& get(u64 id) {
			return utl::as_mutable(utl::as_const(*this).get(id));
		}
		
		T const& get(u64 id) const {
			SC_EXPECT(id != 0, "Invalid TypeID");
			
			auto const itr = _elements.find(id);
			if (itr == _elements.end()) {
				throw std::runtime_error(utl::strcat("Can't find ", T::elementName(), " with ID ", id));
			}
			return *itr->second;
		}
		
		template <typename... Args>
		T& emplace(u64 id, Args&&... args) {
			auto [itr, success] = _elements.insert({ id, std::make_unique<T>(std::forward<Args>(args)...) });
			return *itr->second;
		}
		
	private:
		utl::hashmap<u64, std::unique_ptr<T>> _elements;
	};
	
}

#endif // SCATHA_COMMON_ELEMENTTABLE_H_

