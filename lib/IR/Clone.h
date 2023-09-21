#ifndef SCATHA_IR_CLONE_H_
#define SCATHA_IR_CLONE_H_

#include <concepts>

#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class CloneValueMap {
public:
    void add(Value* oldValue, Value* newValue) { map[oldValue] = newValue; }

    template <typename T>
    T* operator()(T* value) const {
        auto itr = map.find(value);
        if (itr == map.end()) {
            return value;
        }
        return cast<T*>(itr->second);
    }

private:
    utl::hashmap<Value*, Value*> map;
};

SCTEST_API UniquePtr<Instruction> clone(Context& context, Instruction* inst);

template <std::derived_from<Instruction> Inst>
SCTEST_API UniquePtr<Inst> clone(Context& context, Inst* inst) {
    return uniquePtrCast<Inst>(clone(context, static_cast<Instruction*>(inst)));
}

SCTEST_API UniquePtr<BasicBlock> clone(Context& context, BasicBlock* BB);

SCTEST_API UniquePtr<BasicBlock> clone(Context& context,
                                       BasicBlock* BB,
                                       CloneValueMap& map);

SCTEST_API UniquePtr<Function> clone(Context& context, Function* function);

} // namespace scatha::ir

#endif // SCATHA_IR_CLONE_H_
