#ifndef SCATHA_IRGEN_CALLINGCONVENTION_H_
#define SCATHA_IRGEN_CALLINGCONVENTION_H_

#include <array>
#include <iosfwd>
#include <span>

#include <range/v3/view.hpp>
#include <utl/ipp.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/Fwd.h"
#include "IRGen/Value.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Description of how a value is passed to and returned from function calls.
class PassingConvention {
public:
    PassingConvention(sema::Type const* type,
                      std::span<ValueLocation const> locations):
        _type(type), _locs{} {
        for (auto [index, loc]: locations | ranges::views::enumerate) {
            _locs[index] = loc;
        }
        _numParams = (unsigned char)locations.size();
    }

    PassingConvention(sema::Type const* type,
                      std::initializer_list<ValueLocation> locations):
        PassingConvention(type, std::span(locations)) {}

    /// \Returns the sema type of this argument
    sema::Type const* type() const { return _type; }

    /// \Returns the location of the argument. Either `Register` or `Stack`
    std::span<ValueLocation const> locations() const {
        return std::span(_locs).subspan(0, _numParams);
    }

    ///
    ValueLocation location(size_t index) const {
        SC_EXPECT(index < _numParams);
        return _locs[index];
    }

    /// \Returns the location of the argument at the call site. This is `Memory`
    /// if `semaType()` is a reference type, otherwise it is `location()`
    utl::small_vector<ValueLocation, 2> locationsAtCallsite() const;

    ///
    ValueLocation locationAtCallsite(size_t index) const {
        return locationsAtCallsite()[index];
    }

    /// \Returns the number of IR parameters that are occupied by this value
    size_t numParams() const { return locations().size(); }

private:
    sema::Type const* _type;
    std::array<ValueLocation, 2> _locs;
    unsigned char _numParams;
};

/// Print \p PC to \p ostream
std::ostream& operator<<(std::ostream& ostream, PassingConvention PC);

/// Description of how a function expects its arguments and return value to be
/// passed
class CallingConvention {
public:
    CallingConvention() = default;

    explicit CallingConvention(sema::Type const* returnType,
                               ValueLocation retLocation,
                               std::span<PassingConvention const> args):
        ret(returnType, retLocation), args(args | ToSmallVector<>) {}

    ///
    sema::Type const* returnType() const { return ret.pointer(); }

    ///
    ValueLocation returnLocation() const { return ret.integer(); }

    ///
    ValueLocation returnLocationAtCallsite() const;

    /// `PassingConvention`s of the arguments
    std::span<PassingConvention const> arguments() const { return args; }

    /// `PassingConvention` of the argument at index \p index
    PassingConvention argument(size_t index) const {
        return arguments()[index];
    }

private:
    utl::ipp<sema::Type const*, ValueLocation, 1> ret;
    utl::small_vector<PassingConvention> args;
};

/// Print \p CC to \p str
void print(CallingConvention const& CC, std::ostream& str);

/// Print \p CC to `std::cout`
void print(CallingConvention const& CC);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_CALLINGCONVENTION_H_
