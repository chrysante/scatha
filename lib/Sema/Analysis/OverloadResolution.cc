#include "Sema/Analysis/OverloadResolution.h"

#include <optional>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

///  \returns The maximum conversion rank if all arguments are convertible to
/// the parameters  `std::nullopt` otherwise
static std::optional<int> signatureMatch(
    OverloadResolutionResult& result,
    std::span<QualType const> argTypes,
    std::span<Value const* const> constantArgs,
    std::span<QualType const> paramTypes,
    bool isMemberCall) {
    if (paramTypes.size() != argTypes.size()) {
        return std::nullopt;
    }
    SC_ASSERT(argTypes.size() == constantArgs.size(), "");
    int maxRank = 0;
    for (auto [index, argType, constArg, paramType]:
         ranges::views::zip(ranges::views::iota(0),
                            argTypes,
                            constantArgs,
                            paramTypes))
    {
        if (!paramType || !argType) {
            return std::nullopt;
        }
        using enum ConversionKind;
        auto convKind = isMemberCall && index == 0 ? Explicit : Implicit;
        auto conversion =
            computeConversion(convKind, argType, constArg, paramType);
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

static OverloadResolutionResult performORImpl(
    OverloadSet* overloadSet,
    std::span<QualType const> argTypes,
    std::span<Value const* const> constArgs,
    bool isMemberCall) {
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
        auto rank = signatureMatch(match,
                                   argTypes,
                                   constArgs,
                                   F->argumentTypes(),
                                   isMemberCall);
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
    auto firstHigherRank = ranges::find_if(ranks, [=](auto pair) {
        return pair.rank != lowestRank;
    });
    ranks.erase(firstHigherRank, ranks.end());

    switch (ranks.size()) {
    case 0:
        return makeError<NoMatchingFunction>(overloadSet);
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

OverloadResolutionResult sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<QualType const> argTypes,
    bool isMemberCall) {
    return performORImpl(overloadSet,
                         argTypes,
                         utl::small_vector<Value const*>(argTypes.size()),
                         isMemberCall);
}

/// \overload for expressions
OverloadResolutionResult sema::performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<ast::Expression const* const> arguments,
    bool isMemberCall) {
    utl::small_vector<QualType> argTypes;
    utl::small_vector<Value const*> argValues;
    argTypes.reserve(arguments.size());
    argValues.reserve(arguments.size());
    for (auto* arg: arguments) {
        argTypes.push_back(arg->type());
        argValues.push_back(arg->constantValue());
    }
    return performORImpl(overloadSet, argTypes, argValues, isMemberCall);
}
