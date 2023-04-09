#ifndef SCATHA_IR_DATAFLOW_H_
#define SCATHA_IR_DATAFLOW_H_

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::ir {

struct BasicBlockLiveSets {
    utl::hashset<Value const*> liveIn, liveOut;
};

class LiveSets {
public:
    /// Computes the live-in and live-out sets for each basic block of function
    /// \p F
    SCATHA_TESTAPI static LiveSets compute(Function const& F);

    BasicBlockLiveSets const& live(BasicBlock const* BB) const {
        auto itr = sets.find(BB);
        SC_ASSERT(itr != sets.end(), "Not found");
        return itr->second;
    }

    BasicBlockLiveSets const& operator[](BasicBlock const* BB) const {
        return live(BB);
    }

    auto begin() const { return sets.begin(); }

    auto end() const { return sets.end(); }

    bool empty() const { return sets.empty(); }

    bool size() const { return sets.size(); }

private:
    LiveSets() = default;

    utl::hashmap<BasicBlock const*, BasicBlockLiveSets> sets;
};

} // namespace scatha::ir

#endif // SCATHA_IR_DATAFLOW_H_
