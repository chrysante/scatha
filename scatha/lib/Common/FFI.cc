#include "Common/FFI.h"

#include <memory>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>

#include "Common/Ranges.h"

using namespace scatha;

FFITrivialType const FFIType::_sVoid = FFITrivialType(Kind::Void);
FFITrivialType const FFIType::_sInt8 = FFITrivialType(Kind::Int8);
FFITrivialType const FFIType::_sInt16 = FFITrivialType(Kind::Int16);
FFITrivialType const FFIType::_sInt32 = FFITrivialType(Kind::Int32);
FFITrivialType const FFIType::_sInt64 = FFITrivialType(Kind::Int64);
FFITrivialType const FFIType::_sFloat = FFITrivialType(Kind::Float);
FFITrivialType const FFIType::_sDouble = FFITrivialType(Kind::Double);
FFITrivialType const FFIType::_sPointer = FFITrivialType(Kind::Pointer);

namespace {

struct StructKey {
    utl::small_vector<FFIType const*> elems;

    bool operator==(StructKey const&) const = default;
};

} // namespace

template <>
struct std::hash<StructKey> {
    size_t operator()(StructKey const& key) const {
        return utl::hash_combine_range(key.elems.begin(), key.elems.end());
    }
};

FFIType const* FFIType::Struct(std::span<FFIType const* const> elementTypes) {
    static utl::hashmap<StructKey, std::unique_ptr<FFIType>> map;
    StructKey key = { elementTypes | ToSmallVector<> };
    auto& p = map[key];
    if (!p) {
        p = std::make_unique<FFIStructType>(std::move(key.elems));
    }
    return p.get();
}

FFIStructType::FFIStructType(std::span<FFIType const* const> elementTypes):
    FFIType(Kind::Struct), elems(elementTypes | ToSmallVector<>) {}

ForeignFunctionInterface::ForeignFunctionInterface(
    std::string name, std::span<FFIType const* const> argTypes,
    FFIType const* retType):
    _name(std::move(name)) {
    sig.push_back(retType);
    ranges::copy(argTypes, std::back_inserter(sig));
}
