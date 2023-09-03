#ifndef SCATHA_AST_LOWERING_CALLINGCONVENTION_H_
#define SCATHA_AST_LOWERING_CALLINGCONVENTION_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/Lowering/Value.h"
#include "Common/Ranges.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::ast {

/// Description of how a value is passed to and returned from function calls.
class PassingConvention {
public:
    PassingConvention(ValueLocation loc, size_t numParams):
        _loc(loc), _numParams(utl::narrow_cast<uint16_t>(numParams)) {}

    /// Location type of the argument. Either `Register` or `Stack`
    ValueLocation location() const { return _loc; }

    /// The number of parameters used to pass this value
    size_t numParams() const { return _numParams; }

private:
    ValueLocation _loc;
    uint16_t _numParams;
};

/// Description of how a function expects its arguments and return value to be
/// passed
class CallingConvention {
public:
    CallingConvention() = default;

    explicit CallingConvention(PassingConvention returnValue,
                               std::span<PassingConvention const> args):
        _args(ranges::views::concat(std::span(&returnValue, 1), args) |
              ToSmallVector<>) {}

    /// `PassingConvention` of the return value
    PassingConvention returnValue() const {
        SC_ASSERT(!_args.empty(), "Invalid CC");
        return _args[0];
    }

    /// `PassingConvention`s of the arguments
    std::span<PassingConvention const> arguments() const {
        return std::span(_args).subspan(1);
    }

    /// `PassingConvention` of the argument at index \p index
    PassingConvention argument(size_t index) const {
        return arguments()[index];
    }

private:
    utl::small_vector<PassingConvention> _args;
};

} // namespace scatha::ast

#endif // SCATHA_AST_LOWERING_CALLINGCONVENTION_H_
