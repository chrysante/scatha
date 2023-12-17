#ifndef SCATHA_COMMON_FOREIGNFUNCTIONDECL_H_
#define SCATHA_COMMON_FOREIGNFUNCTIONDECL_H_

#include <bit>
#include <functional> // For std::hash
#include <string>
#include <vector>

#include <scatha/Common/Base.h>

namespace scatha {

/// Represents the address of a foreign function.
struct SCATHA_API ForeignFuncAddress {
    bool operator==(ForeignFuncAddress const&) const = default;

    uint32_t slot  : 11;
    uint32_t index : 21;
};

static_assert(sizeof(ForeignFuncAddress) == 4);

/// Represents a foreign function declaration
struct SCATHA_API ForeignFunctionDecl {
    /// The name of the function
    std::string name;

    /// Index of the foreign library that this function is defined in
    size_t libIndex = 0;

    /// The address of the function
    ForeignFuncAddress address;

    /// Size of the return value
    size_t retType;

    /// Sizes of the function argument types
    std::vector<size_t> argTypes;
};

} // namespace scatha

template <>
struct std::hash<scatha::ForeignFuncAddress> {
    std::size_t operator()(scatha::ForeignFuncAddress addr) const {
        return std::hash<uint32_t>{}(std::bit_cast<std::uint32_t>(addr));
    }
};

#endif // SCATHA_COMMON_FOREIGNFUNCTIONDECL_H_
