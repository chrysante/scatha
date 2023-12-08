#ifndef SCATHA_CODEGEN_VALUEMAP_H_
#define SCATHA_CODEGEN_VALUEMAP_H_

#include <optional>

#include <utl/hashtable.hpp>

#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

/// Maps IR values to corresponding MIR values
class ValueMap {
public:
    /// Access the MIR value mapped to \p key or null if nonesuch exists
    mir::Value* getValue(ir::Value const* key) const;

    /// Insert the pair \p key and \p value into the map
    /// This function traps if \p key is already in the map
    void addValue(ir::Value const* key, mir::Value* value);

    /// Get the MIR pointer value of the IR pointer value \p value
    std::pair<mir::Value*, size_t> getAddress(ir::Value const* key);

    /// Associate the IR pointer value \p key with the pair `{ baseAddr, offset
    /// }` This function traps if \p key is already in the map
    void addAddress(ir::Value const* key,
                    mir::Value* baseAddr,
                    size_t offset = 0);

    /// Get the static data offset of the IR pointer value \p value
    std::optional<uint64_t> getStaticAddress(ir::Value const* key) const;

    /// Associate the IR pointer value \p key with the static data offset \p
    /// offset This function traps if \p key is already in the map
    void addStaticAddress(ir::Value const* key, uint64_t offset);

private:
    /// Maps IR values to MIR values
    utl::hashmap<ir::Value const*, mir::Value*> map;

    /// Maps IR pointer values to MIR pointers (and possibly static non-zero
    /// offsets)
    utl::hashmap<ir::Value const*, std::pair<mir::Value*, size_t>> addressMap;

    /// Maps IR pointer values to offsets into the static data section of the
    /// executable
    utl::hashmap<ir::Value const*, uint64_t> staticDataAddresses;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_VALUEMAP_H_
