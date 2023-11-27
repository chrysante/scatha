#ifndef SCATHA_CODEGEN_VALUEMAP_H_
#define SCATHA_CODEGEN_VALUEMAP_H_

#include <utl/hashtable.hpp>

#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

namespace internal {

template <typename>
struct CFGTypeMap;

template <>
struct CFGTypeMap<ir::Function>: std::type_identity<mir::Function> {};

template <>
struct CFGTypeMap<ir::BasicBlock>: std::type_identity<mir::BasicBlock> {};

template <>
struct CFGTypeMap<ir::Value>: std::type_identity<mir::Value> {};

/// Here we don't have a canonical mapping but since at this stage we only have SSA registers and instructions define SSA registers this seems sensible
template <>
struct CFGTypeMap<ir::Instruction>: std::type_identity<mir::SSARegister> {};

} // namespace internal

/// Maps IR values to corresponding MIR values
class ValueMap {
public:
    /// Access the MIR value mapped to \p key
    /// This function traps if \p key is not in the map
    mir::Value* operator()(ir::Value const* key) const;

    /// \overload for derived types
    template <typename IRType>
    auto* operator()(IRType const* key) const {
        using MIRType = typename internal::CFGTypeMap<IRType>::type;
        auto* baseKey = static_cast<ir::Value const*>(key);
        return cast<MIRType*>((*this)(baseKey));
    }

    /// Insert the pair \p key and \p value into the map
    /// This function traps if \p key is already in the map
    void insert(ir::Value const* key, mir::Value* value);

private:
    utl::hashmap<ir::Value const*, mir::Value*> map;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_VALUEMAP_H_
