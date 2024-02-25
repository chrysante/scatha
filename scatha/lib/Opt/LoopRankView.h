#ifndef SCATHA_OPT_LOOPRANKVIEW_H_
#define SCATHA_OPT_LOOPRANKVIEW_H_

#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "IR/Fwd.h"

namespace scatha::opt {

/// We define the _rank_ of a loop to be its nesting depth.
/// View over the loop nest structure of a function that allows iteration of
/// loop headers along constant ranks. This means traversal of all loops with
/// nesting depth N followed by traversal of all loops with nesting depth N+1
/// (or reverse order). For example the loops of the following function
/// ```
/// function {
///     L1 {
///         L1.1 {}
///         L1.2 {}
///     }
///     L2 {
///         L2.1 {}
///         L2.2 {}
///     }
/// }
/// ```
/// can be traversed in several ways:
/// - Loops of depth 0 followed by loops of depth 1:
///   `L1, L2, L1.1, L1.2, L2.1, L2.2`
/// - Loops of depth 1 followed by loops of depth 0:
///   `L1.1, L1.2, L2.1, L2.2, L1, L2`
class LoopRankView {
public:
    explicit LoopRankView() = default;

    /// Compute the rank view for \p function
    static LoopRankView Compute(ir::Function& function);

    /// Compute the rank view for \p function but only track loops whose header
    /// satisfies \p headerPredicate
    static LoopRankView Compute(
        ir::Function& function,
        utl::function_view<bool(ir::BasicBlock const*)> headerPredicate);

    /// Range helpers @{
    auto begin() const { return _ranks.begin(); }
    auto end() const { return _ranks.end(); }
    size_t size() const { return _ranks.size(); }
    /// }

private:
    utl::vector<utl::small_vector<ir::BasicBlock*>> _ranks;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_LOOPRANKVIEW_H_
