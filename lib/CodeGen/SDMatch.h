#ifndef SCATHA_CG_SDMATCH_H_
#define SCATHA_CG_SDMATCH_H_

#include <functional>

#include <utl/vector.hpp>

#include "CodeGen/Resolver.h"
#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::cg {

class SelectionNode;

template <typename T>
struct Matcher;

/// Interface for a DAG match case. DAG match cases exist for every IR instruction
/// type
class MatcherBase {
public:
    ///
    bool match(ir::Instruction const& inst, SelectionNode& node) const;

    ///
    void init(Resolver resolver) { this->resolver = resolver; }

protected:
    using CaseImpl =
        std::function<bool(ir::Instruction const&, SelectionNode&)>;

    void addMatchCase(CaseImpl matchCase) {
        matchCases.push_back(std::move(matchCase));
    }

    Resolver resolver;

private:
    utl::small_vector<CaseImpl> matchCases;
};

} // namespace scatha::cg

/// Implementation of `SD_MATCH_CASE`
#define SD_MATCH_CASE_IMPL(CaseName, ...)                                      \
    int SC_CONCAT(MatcherName, Init) : 1 = [&] {                               \
        auto caseWrapper = [this](ir::Instruction const& inst,                 \
                                  SelectionNode& node) -> bool {               \
            using InstType =                                                   \
                typename internal::MatcherToInstType<Matcher>::type;           \
            return CaseName(cast<InstType const&>(inst), node);                \
        };                                                                     \
        this->addMatchCase(caseWrapper);                                       \
        return 0;                                                              \
    }();                                                                       \
    bool CaseName(__VA_ARGS__) const

/// Declares a match case for an IR instruction type. This is meant to be used
/// within a `SD_MATCHER(...) { ... }` block
#define SD_MATCH_CASE(...)                                                     \
    SD_MATCH_CASE_IMPL(SC_CONCAT(MatchCase, __LINE__), __VA_ARGS__)

// ===-----------------------------------------------------------------------===
// ===--- Inline implementation ---------------------------------------------===
// ===-----------------------------------------------------------------------===

namespace scatha::cg::internal {

template <typename>
struct MatcherToInstType;

template <typename Inst>
struct MatcherToInstType<cg::Matcher<Inst>>: std::type_identity<Inst> {};

} // namespace scatha::cg::internal

#endif // SCATHA_CG_SDMATCH_H_
