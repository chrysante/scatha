#ifndef SCATHA_CG_SDMATCH_H_
#define SCATHA_CG_SDMATCH_H_

#include <functional>

#include <range/v3/algorithm.hpp>
#include <utl/vector.hpp>

#include "CodeGen/Resolver.h"
#include "CodeGen/SelectionDAG.h"
#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::cg {

class MatcherBase;

template <typename T>
struct Matcher;

namespace internal {

using CaseImpl = std::function<bool(ir::Instruction const&, SelectionNode&)>;

void addMatchCase(MatcherBase&, CaseImpl);

} // namespace internal

/// Interface for a DAG matcher. DAG matchers must exist for every IR
/// instruction type
class MatcherBase: public Resolver {
public:
    ///
    bool match(ir::Instruction const& inst, SelectionNode& node) const;

    ///
    void init(mir::Context& CTX, SelectionDAG& DAG, Resolver resolver) {
        _ctx = &CTX;
        _dag = &DAG;
        Resolver::operator=(std::move(resolver));
    }

protected:
    /// \Returns the MIR context
    mir::Context& CTX() const { return *_ctx; }

    /// \Returns the DAG
    SelectionDAG& DAG() const { return *_dag; }

    /// \Returns the DAG node of \p inst
    SelectionNode* DAG(ir::Instruction const* inst) const {
        return DAG()[inst];
    }

    /// Tests if \p load has no execution dependencies on \p value. Only
    /// in that case can we merge the load into the using instruction
    bool canDeferLoad(ir::Load const* load, ir::Value const* value) const;

    /// \overload
    template <ranges::range R>
        requires std::convertible_to<ranges::range_value_t<R>, ir::Value const*>
    bool canDeferLoad(ir::Load const* load, R&& r) const {
        return ranges::all_of(r, [&](auto* value) {
            return canDeferLoad(load, value);
        });
    }

private:
    friend void internal::addMatchCase(MatcherBase&, internal::CaseImpl);

    utl::small_vector<internal::CaseImpl, 32> matchCases;
    mir::Context* _ctx = nullptr;
    SelectionDAG* _dag = nullptr;
};

} // namespace scatha::cg

/// Implementation of `SD_MATCH_CASE`
#define SD_MATCH_CASE_IMPL(CaseName, ...)                                      \
    int SC_CONCAT(CaseName, _Init) : 1 = [&] {                                 \
        auto caseWrapper = [this](ir::Instruction const& inst,                 \
                                  SelectionNode& node) -> bool {               \
            using InstType =                                                   \
                typename internal::MatcherToInstType<Matcher>::type;           \
            return CaseName(cast<InstType const&>(inst), node);                \
        };                                                                     \
        ::scatha::cg::internal::addMatchCase(*this, caseWrapper);              \
        return 0;                                                              \
    }() & 0;                                                                   \
    bool CaseName(__VA_ARGS__)

/// Declares a match case for an IR instruction type. This is meant to be used
/// within a `SD_MATCHER(...) { ... }` block
#define SD_MATCH_CASE(...)                                                     \
    SD_MATCH_CASE_IMPL(SC_CONCAT(MatchCase_, __LINE__), __VA_ARGS__)

// ===-----------------------------------------------------------------------===
// ===--- Inline implementation ---------------------------------------------===
// ===-----------------------------------------------------------------------===

inline void scatha::cg::internal::addMatchCase(MatcherBase& matcher,
                                               CaseImpl matchCase) {
    matcher.matchCases.push_back(std::move(matchCase));
}

namespace scatha::cg::internal {

template <typename>
struct MatcherToInstType;

template <typename Inst>
struct MatcherToInstType<cg::Matcher<Inst>>: std::type_identity<Inst> {};

} // namespace scatha::cg::internal

#endif // SCATHA_CG_SDMATCH_H_
