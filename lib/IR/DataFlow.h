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

    /// \Returns Live sets of basic block \p BB or `nullptr` if \p BB does not have live sets.
    /// This can occur if a block is unreachable.
    BasicBlockLiveSets const* find(BasicBlock const* BB) const {
        auto itr = sets.find(BB);
        if (itr == sets.end()) {
            return nullptr;
        }
        return &itr->second;
    }

    auto begin() const { return sets.begin(); }

    auto end() const { return sets.end(); }

    bool empty() const { return sets.empty(); }

    bool size() const { return sets.size(); }

private:
    utl::hashmap<BasicBlock const*, BasicBlockLiveSets> sets;
};

} // namespace scatha::ir

#endif // SCATHA_IR_DATAFLOW_H_
