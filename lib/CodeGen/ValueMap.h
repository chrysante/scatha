#ifndef SCATHA_CODEGEN_VALUEMAP_H_
#define SCATHA_CODEGEN_VALUEMAP_H_

#include <utl/hashtable.hpp>

#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

/// Maps IR values to corresponding MIR values
class ValueMap {
public:
    /// Access the MIR value mapped to \p key or null if nonesuch exists
    mir::Value* operator()(ir::Value const* key) const;

    /// Insert the pair \p key and \p value into the map
    /// This function traps if \p key is already in the map
    void insert(ir::Value const* key, mir::Value* value);

private:
    utl::hashmap<ir::Value const*, mir::Value*> map;
};

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_VALUEMAP_H_
