#include "Sema/Analysis/OverloadResolution.h"

#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

///  \returns The maximum conversion rank if all arguments are convertible to
/// the parameters  `std::nullopt` otherwise
static std::optional<int> signatureMatch(
    std::span<QualType const* const> parameters,
    std::span<ast::Expression const* const> arguments,
    bool isMemberCall) {
    if (parameters.size() != arguments.size()) {
        return std::nullopt;
    }
    int maxRank = 0;
    for (auto [index, param, arg]:
         ranges::views::zip(ranges::views::iota(0), parameters, arguments))
    {
        if (!param || !arg) {
            return std::nullopt;
        }
        auto const rank = isMemberCall && index == 0 ?
                              explicitConversionRank(arg, param) :
                              implicitConversionRank(arg, param);
        if (!rank) {
            return std::nullopt;
        }
        maxRank = std::max(maxRank, *rank);
    }
    return maxRank;
}

namespace {

struct Match {
    Function* function;
    int rank;
};

} // namespace

Expected<Function*, OverloadResolutionError*> sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<ast::Expression const* const> argumentTypes,
    bool isMemberCall) {
    utl::small_vector<Match> matches;
    for (auto* F: *overloadSet) {
        if (auto rank =
                signatureMatch(F->argumentTypes(), argumentTypes, isMemberCall))
        {
            matches.push_back({ F, *rank });
        }
    }
    if (matches.empty()) {
        return new NoMatchingFunction(overloadSet);
    }
    if (matches.size() == 1) {
        return matches.front().function;
    }
    ranges::sort(matches, ranges::less{}, [](Match match) {
        return match.rank;
    });
    int const lowestRank = matches.front().rank;
    auto firstHigherRank = ranges::find_if(matches, [=](Match match) {
        return match.rank != lowestRank;
    });
    matches.erase(firstHigherRank, matches.end());

    switch (matches.size()) {
    case 0:
        return new NoMatchingFunction(overloadSet);
    case 1:
        return matches.front().function;
    default: {
        auto functions = matches | ranges::views::transform([](Match match) {
                             return match.function;
                         }) |
                         ranges::to<utl::small_vector<Function*>>;
        return new AmbiguousOverloadResolution(overloadSet,
                                               std::move(functions));
    }
    }
}
