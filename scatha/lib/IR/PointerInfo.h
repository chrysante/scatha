#ifndef SCATHA_IR_POINTERINFO_H_
#define SCATHA_IR_POINTERINFO_H_

#include <optional>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Parameters to initialize `PointerInfo`
struct PointerInfoDesc {
    ssize_t align = 0;
    std::optional<ssize_t> validSize;
    Value* provenance = nullptr;
    std::optional<ssize_t> staticProvenanceOffset;
    bool guaranteedNotNull = false;
};

/// Statically known pointer meta data
class PointerInfo {
public:
    PointerInfo() = default;

    PointerInfo(PointerInfoDesc desc);

    /// The minimum alignment requirement that can be assumed for this pointer
    ssize_t align() const { return (ssize_t)_align; }

    /// The number of bytes which are dereferencable through this pointer if
    /// known statically
    std::optional<ssize_t> validSize() const {
        return _validSize != InvalidSize ? std::optional(_validSize) :
                                           std::nullopt;
    }

    /// \Returns the value that this pointer originates from. This could be a
    /// function argument, an alloca instruction or a dynamic allocation
    /// For example in the code
    ///
    ///     %alloc = alloca i32, i32 5
    ///     %elem = getelementptr i32, ptr %alloc, i32 2
    ///
    /// `%elem` has provenance `%alloc`
    Value* provenance() const { return _prov; }

    /// \Returns the statically known offset in bytes of this pointer from its
    /// provenance or `std::nullopt`
    std::optional<ssize_t> staticProvencanceOffset() const {
        return _staticProvOffset != InvalidSize ?
                   std::optional<ssize_t>(_staticProvOffset) :
                   std::nullopt;
    }

    /// \Returns `true` if this pointer info has provenance and static
    /// provenance offset info available
    bool hasProvAndStaticOffset() const {
        return _prov && staticProvencanceOffset().has_value();
    }

    ///
    void setProvenance(Value* p, std::optional<ssize_t> staticOffset);

    /// \Returns `true` if this pointer is guaranteed not to be null
    bool guaranteedNotNull() const { return _guaranteedNotNull; }

private:
    static constexpr ssize_t InvalidSize = std::numeric_limits<ssize_t>::min();

    uint8_t _align = 0;
    bool _guaranteedNotNull = false;
    Value* _prov = nullptr;
    ssize_t _validSize = InvalidSize;
    ssize_t _staticProvOffset = InvalidSize;
};

} // namespace scatha::ir

#endif // SCATHA_IR_POINTERINFO_H_
