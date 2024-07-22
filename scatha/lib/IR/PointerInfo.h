#ifndef SCATHA_IR_POINTERINFO_H_
#define SCATHA_IR_POINTERINFO_H_

#include <optional>

#include <utl/ipp.hpp>

#include "Common/Base.h"
#include "IR/Fwd.h"
#include "IR/ValueRef.h"

namespace scatha::ir {

///
class PointerProvenance {
public:
    static PointerProvenance Static(Value* value) {
        return PointerProvenance(value, true);
    }

    static PointerProvenance Dynamic(Value* value) {
        return PointerProvenance(value, false);
    }

    PointerProvenance(Value* value, bool isStatic): _value(value, isStatic) {}

    /// \Returns `true` if the provenance is the statically known origin of the
    /// pointer, e.g., an `alloca` instruction or a call to `__builtin_alloc`.
    /// Pointers whose origin is unknown are "dynamic"
    bool isStatic() const { return _value.integer(); }

    Value* value() const { return _value.pointer(); }

private:
    utl::ipp<Value*, bool, 1> _value;
};

/// Parameters to initialize `PointerInfo`
struct PointerInfoDesc {
    ssize_t align = 1;
    std::optional<ssize_t> validSize;
    PointerProvenance provenance;
    std::optional<ssize_t> staticProvenanceOffset;
    bool guaranteedNotNull = false;
    bool hasStaticProvenance = false;
    bool nonEscaping = false;
};

/// Statically known pointer meta data
class PointerInfo {
public:
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
    PointerProvenance provenance() const {
        return { _prov.value(), _staticProv };
    }

    /// \Returns the statically known offset in bytes of this pointer from its
    /// provenance or `std::nullopt`
    std::optional<ssize_t> staticProvenanceOffset() const {
        return _staticProvOffset != InvalidSize ?
                   std::optional<ssize_t>(_staticProvOffset) :
                   std::nullopt;
    }

    ///
    void setProvenance(PointerProvenance p,
                       std::optional<ssize_t> staticOffset);

    /// \Returns `true` if this pointer is a function-local value and does not
    /// espace the scope of the function, i.e., it is not stored to memory and
    /// not passed to function calls
    bool isNonEscaping() const { return _nonEscaping; }

    ///
    void setNonEscaping(bool value = true) { _nonEscaping = value; }

    /// \Returns `true` if this pointer is guaranteed not to be null
    bool guaranteedNotNull() const { return _guaranteedNotNull; }

private:
    static constexpr ssize_t InvalidSize = std::numeric_limits<ssize_t>::min();

    uint8_t _align = 0;
    bool _guaranteedNotNull = false;
    bool _nonEscaping = false;
    bool _staticProv = false;
    ValueRef _prov;
    ssize_t _validSize = InvalidSize;
    ssize_t _staticProvOffset = InvalidSize;
};

} // namespace scatha::ir

#endif // SCATHA_IR_POINTERINFO_H_
