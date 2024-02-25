#ifndef SCATHA_IR_CLONE_H_
#define SCATHA_IR_CLONE_H_

#include <concepts>
#include <span>

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Maps values to values in cloned code
class CloneValueMap {
public:
    /// Insert a key-value pair into the map
    void add(Value* oldValue, Value* newValue) { map[oldValue] = newValue; }

    /// Maps \p key to its cloned counterpart. If nonesuch exists \p key will be
    /// returned
    template <typename T>
    T* operator()(T* key) const {
        auto itr = map.find(key);
        if (itr == map.end()) {
            return key;
        }
        return cast<T*>(itr->second);
    }

    /// Maps \p key to its cloned counterpart. Traps if nonesuch exists
    template <typename T>
    T* operator[](T* key) const {
        auto itr = map.find(key);
        SC_ASSERT(itr != map.end(), "Not found");
        return cast<T*>(itr->second);
    }

    /// Inverse map.
    /// \Warning This is linear in the number of elements in the map
    template <typename T>
    T* inverse(T* V) const {
        for (auto& [key, value]: map) {
            if (value == V) {
                return cast<T*>(key);
            }
        }
        return V;
    }

private:
    utl::hashmap<Value*, Value*> map;
};

/// \Returns a clone of the instruction \p inst
SCTEST_API UniquePtr<Instruction> clone(Context& context, Instruction* inst);

/// \Returns a clone of the instruction \p inst downcast to the type of the
/// argument
template <std::derived_from<Instruction> Inst>
SCTEST_API UniquePtr<Inst> clone(Context& context, Inst* inst) {
    return uniquePtrCast<Inst>(clone(context, static_cast<Instruction*>(inst)));
}

/// \Returns a clone of the basic block \p BB
SCTEST_API UniquePtr<BasicBlock> clone(Context& context, BasicBlock* BB);

/// \overload accepting a clone map
SCTEST_API UniquePtr<BasicBlock> clone(Context& context, BasicBlock* BB,
                                       CloneValueMap& map);

/// Result structure for `cloneRegion()`
struct CloneRegionResult {
    /// The clone map used in the process
    CloneValueMap map;

    /// The cloned basic blocks
    utl::small_vector<BasicBlock*> clones;
};

/// Clones every block in the list \p region and inserts them in order before \p
/// insertPoint
CloneRegionResult cloneRegion(Context& context, BasicBlock const* insertPoint,
                              std::span<BasicBlock* const> region);

/// \Returns a clone of the function \p function
SCTEST_API UniquePtr<Function> clone(Context& context, Function* function);

} // namespace scatha::ir

#endif // SCATHA_IR_CLONE_H_
