#ifndef SCATHA_MIR_HASH_H_
#define SCATHA_MIR_HASH_H_

#include <bit>

#include <utl/hash.hpp>

#include "MIR/Fwd.h"

template <>
struct std::hash<scatha::mir::MemAddrConstantData> {
    std::size_t operator()(scatha::mir::MemAddrConstantData data) const {
        return std::hash<uint16_t>{}(std::bit_cast<uint16_t>(data));
    }
};

template <typename V>
struct std::hash<scatha::mir::MemoryAddressImpl<V>> {
    std::size_t operator()(
        scatha::mir::MemoryAddressImpl<V> const& addr) const {
        return utl::hash_combine(addr.baseAddress(),
                                 addr.dynOffset(),
                                 addr.constantData());
    }
};

template <>
struct std::hash<scatha::mir::ExtFuncAddress> {
    std::size_t operator()(scatha::mir::ExtFuncAddress addr) const {
        return std::hash<uint32_t>{}(std::bit_cast<std::uint32_t>(addr));
    }
};

#endif // SCATHA_MIR_HASH_H_
