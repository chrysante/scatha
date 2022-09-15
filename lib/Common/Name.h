#ifndef SCATHA_COMMON_NAME_H_
#define SCATHA_COMMON_NAME_H_

#include <algorithm>
#include <functional> // for std::hash
#include <string>

#include "Basic/Basic.h"

namespace scatha {
	
	enum class NameCategory {
		None = 0, Type, Value, Namespace, Function,
		_count
	};
	
	std::string_view toString(NameCategory);
	
	enum class TypeID: u64 { Invalid = 0 };
	
	class NameID {
	public:
		NameID() = default;
		explicit NameID(u64 id, NameCategory category):
			_id(id),
			_category(category)
		{}
		
		u64 id() const { return _id; }
		TypeID toTypeID() const { return TypeID(_id);  }
		NameCategory category() const { return _category; }
	
		bool operator==(NameID const& rhs) const { return id() == rhs.id(); }
		
	private:
		u64 _id{};
		NameCategory _category{};
	};
	
	class QualifiedName {
		explicit QualifiedName(std::string v): value(std::move(v)) {
			SC_ASSERT(std::find(value.begin(), value.end(), '.') == value.end(), "value must be an unqualified identifier");
		}

		QualifiedName& operator+=(QualifiedName const& rhs)& {
			*this += rhs.value;
			levels += rhs.levels;
			return *this;
		}
		
		QualifiedName& operator+=(std::string_view rhs)& {
			value.reserve(value.size() + 1 + rhs.size());
			value += '.';
			value += rhs;
			levels += 1;
			return *this;
		}

	private:
		int levels = 0; // aka number of dots in the string
		std::string value;
	};
	
}

template <>
struct std::hash<scatha::NameID> {
	std::size_t operator()(scatha::NameID id) const { return std::hash<scatha::u64>{}(id.id()); }
};

#endif // SCATHA_COMMON_NAME_H_

