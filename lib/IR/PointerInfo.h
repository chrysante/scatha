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

    /// \Returns the value that this pointer originates from. This could be a
    /// function argument, an alloca instruction or a dynamic allocation
    /// For example in the code
    ///
    ///     %alloc = alloca i32, i32 5
    ///     %elem = getelementptr i32, ptr %alloc, i32 2
    ///
    /// `%elem` has provenance `%alloc`
    Value* provenance() const { return prov; }

    /// \Returns the statically known offset in bytes of this pointer from its
    /// provenance or `std::nullopt`
    std::optional<size_t> staticProvencanceOffset() const {
        return hasStaticProvOffset ? std::optional<size_t>(staticProvOffset) :
                                     std::nullopt;
    }

    ///
    void setProvenance(Value* p,
                       std::optional<size_t> staticOffset = std::nullopt);

private:
    size_t _align  : 9;
    bool _hasRange : 1;
    size_t _range  : 54;
    Value* prov = nullptr;
    uint16_t staticProvOffset : 15 = 0;
    bool hasStaticProvOffset  : 1 = false;
};

} // namespace scatha::ir

#endif // SCATHA_IR_POINTERINFO_H_
