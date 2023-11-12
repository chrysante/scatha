#ifndef SCATHA_IR_POINTERINFO_H_
#define SCATHA_IR_POINTERINFO_H_

#include <optional>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Statically known pointer meta data
class PointerInfo {
public:
    PointerInfo(size_t minAlign = 1,
                std::optional<size_t> range = std::nullopt):
        _align(minAlign), _hasRange(range), _range(range.value_or(0)) {}

    /// The minimum alignment requirement that can be assumed for this pointer
    size_t minAlign() const { return _align; }

    /// The number of bytes which are dereferencable through this pointer if
    /// known statically
    std::optional<size_t> range() const {
        return _hasRange ? std::optional(_range) : std::nullopt;
    }

private:
    size_t _align  : 9;
    bool _hasRange : 1;
    size_t _range  : 54;
};

static_assert(sizeof(PointerInfo) == 8, "We try to keep this small");

/// Retrieve pointer info of \p value. Only valid if the type of \p value is
/// `ptr`
PointerInfo getPointerInfo(Value const& value);

} // namespace scatha::ir

#endif // SCATHA_IR_POINTERINFO_H_
