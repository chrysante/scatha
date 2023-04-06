#ifndef SCATHA_IR_DATAFLOW_H_
#define SCATHA_IR_DATAFLOW_H_

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::ir {

class LiveSets;

///
///
///
SCATHA_TESTAPI LiveSets computeLiveSets(Function const& F);

class LiveSets {
    static auto& getImpl(auto& map, auto* BB) {
        auto itr = map.find(BB);
        SC_ASSERT(itr != map.end(), "Not found");
        return itr->second;
    }

public:
    utl::hashset<Instruction const*> const& liveIn(BasicBlock const* BB) const {
        return getImpl(in, BB);
    }

    utl::hashset<Instruction const*> const& liveOut(
        BasicBlock const* BB) const {
        return getImpl(out, BB);
    }

private:
    friend LiveSets ir::computeLiveSets(Function const&);

    LiveSets() = default;

    utl::hashmap<BasicBlock const*, utl::hashset<Instruction const*>> in, out;
};

} // namespace scatha::ir

#endif // SCATHA_IR_DATAFLOW_H_
