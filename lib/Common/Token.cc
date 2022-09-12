#include "Common/Token.h"

#include <ostream>
#include <array>

#include <utl/__base.hpp>
#include <utl/__debug.hpp>

namespace utl {
	
	// Not sure what to think of this
	template <typename E, std::size_t N = (std::size_t)E::_count>
	struct enum_serializer {
		constexpr enum_serializer(std::pair<E, char const*> const (&a)[N]): __data{} {
			if constexpr (requires{ E::_count; }) {
				static_assert(N == (std::size_t)E::_count);
			}
			for (std::size_t i = 0; i < N; ++i) {
				__utl_assert((std::size_t)a[i].first == i, "Make sure every enum case is handled");
				__data[i] = a[i].second;
			}
		}
		
		constexpr std::string_view operator[](E i) const { return __data[(std::size_t)i]; }
		
		std::array<std::string_view, N> __data;
	};
	
	template <typename... E, std::size_t... N>
	enum_serializer(std::pair<E, char const[N]>...) -> enum_serializer<std::common_type_t<E...>, sizeof...(E)>;

#define UTL_SERIALIZE_ENUM(e, ...)                                \
	[&](auto x) {                                                 \
		using E = std::decay_t<decltype(x)>;                      \
		constexpr utl::enum_serializer<E> data = { __VA_ARGS__ }; \
		__utl_assert((std::size_t)x < data.__data.size(),         \
					 "Invalid enum case");                        \
		return data[x];                                           \
	}(e)
	
}

namespace scatha {
	
	std::ostream& operator<<(std::ostream& str, TokenType t) {
		using enum TokenType;
	
		return str << UTL_SERIALIZE_ENUM(t, {
			{ Identifier,     "Identifier" },
			{ NumericLiteral, "NumericLiteral" },
			{ StringLiteral,  "StringLiteral" },
			{ Punctuation,    "Punctuation" },
			{ Operator,       "Operator" },
			{ EndOfFile,      "EndOfFile" }
		});
	}
	
	std::ostream& operator<<(std::ostream& str, Token const& t) {
		str << "{ ";
		str << t.sourceLocation.line << ", " << t.sourceLocation.column;
		str << ", " << "TokenType::" << t.type;
		str << ", \"" << t.id << "\"";
		str << " }";
		return str;
	}
	
	
}
