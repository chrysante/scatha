#include "Sema/Analysis/OverloadResolution.h"

#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;

///  \returns The maximum conversion rank if all arguments are convertible to
/// the parameters  `std::nullopt` otherwise
static std::optional<int> signatureMatch(
    OverloadResolutionResult& result,
    std::span<ast::Expression const* const> args,
    std::span<Type const* const> paramTypes,
    ORKind kind) {
    if (args.size() != paramTypes.size()) {
        return std::nullopt;
    }
    SC_ASSERT(args.size() == paramTypes.size(), "");
    int maxRank = 0;
    for (auto [index, expr, paramType]:
         ranges::views::zip(ranges::views::iota(0), args, paramTypes))
    {
        if (!expr->type() || !paramType) {
            return std::nullopt;
        }
        using enum ConversionKind;
        auto convKind =
            kind == ORKind::MemberFunction && index == 0 ? Explicit : Implicit;
        auto conversion = computeConversion(convKind,
                                            expr->type(),
                                            expr->valueCategory(),
                                            getQualType(paramType),
                                            refToLValue(paramType),
                                            expr->constantValue());
        if (!conversion) {
            return std::nullopt;
        }
        result.conversions.push_back(*conversion);
        int rank = computeRank(*conversion);
        maxRank = std::max(maxRank, rank);
    }
    return maxRank;
}

template <typename Error, typename... Args>
static OverloadResolutionResult makeError(Args&&... args) {
    OverloadResolutionResult result{};
    result.error = std::make_unique<Error>(std::forward<Args>(args)...);
    return result;
}

OverloadResolutionResult sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<ast::Expression const* const> arguments,
    ORKind kind) {
    utl::small_vector<OverloadResolutionResult, 4> results;
    /// Contains ranks and the index of the matching result structure in
    /// `results`
    struct RankIndexPair {
        int rank;
        uint32_t index;
    };
    utl::small_vector<RankIndexPair> ranks;
    for (auto* F: *overloadSet) {
        OverloadResolutionResult match{ .function = F };
        auto rank = signatureMatch(match, arguments, F->argumentTypes(), kind);
        if (rank) {
            ranks.push_back(
                { *rank, utl::narrow_cast<uint32_t>(results.size()) });
            results.push_back(std::move(match));
        }
    }
    if (results.empty()) {
        return makeError<NoMatchingFunction>(overloadSet);
    }
    if (results.size() == 1) {
        return std::move(results.front());
    }
    ranges::sort(ranks, ranges::less{}, [](auto pair) { return pair.rank; });
    int const lowestRank = ranks.front().rank;
    /// TODO: Use lower_bound or similar here
    auto firstHigherRank = ranges::find_if(ranks, [=](auto pair) {
        return pair.rank != lowestRank;
    });
    ranks.erase(firstHigherRank, ranks.end());

    switch (ranks.size()) {
    case 0:
        SC_UNREACHABLE();
    case 1:
        return std::move(results[ranks.front().index]);
    default: {
        auto functions =
            results |
            ranges::views::transform(&OverloadResolutionResult::function) |
            ToSmallVector<>;
        return makeError<AmbiguousOverloadResolution>(overloadSet,
                                                      std::move(functions));
    }
    }
}
