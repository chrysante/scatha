#ifndef SCATHADB_UTIL_TINYBOOLSTACK_H_
#define SCATHADB_UTIL_TINYBOOLSTACK_H_

#include <bit>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sdb {

struct TinyPtrStackAssertTraits {
    static void reportOverflow(std::string_view) { assert(false); }

    static void reportUnderflow(std::string_view) { assert(false); }
};

struct TinyPtrStackThrowTraits {
    static void reportOverflow(std::string_view message) {
        throw std::overflow_error(std::string(message));
    }

    static void reportUnderflow(std::string_view message) {
        throw std::underflow_error(std::string(message));
    }
};

/// Non-allocating tiny bool stack. Has a memory footprint of \p T, stores all
/// values inline. Maximum capacity (with 64 values) is 58 values.
template <typename ValueType = uint64_t,
          typename Traits = TinyPtrStackThrowTraits>
class TinyBoolStack: private Traits {
public:
    static_assert(std::is_unsigned_v<ValueType>,
                  "ValueType must be an unsigned integer type");

    using value_type = ValueType;

    TinyBoolStack() = default;

    explicit constexpr TinyBoolStack(Traits const& traits): Traits(traits) {}

    /// \Returns the number of elements
    constexpr size_t size() const { return bits >> NumDataBits; }

    /// \Returns the maximum number of elements
    static constexpr size_t capacity() { return NumDataBits; }

    /// \Returns `size() == 0`
    constexpr bool empty() const { return size() == 0; }

    /// \Returns true if the stack is full
    constexpr bool full() const { return size() == NumDataBits; }

    /// Pushes \p value onto the stack
    /// \pre `!full()`
    constexpr void push(bool value) {
        if (full()) Traits::reportOverflow("Called push() on full stack");
        size_t sz = size();
        ValueType data = bits & DataMask;
        data |= ValueType(value) << sz;
        bits = packBits(sz + 1, data);
    }

    /// Pops the top element off the stack.
    /// \Returns the top element prior to the pop.
    /// \pre `!empty()`
    constexpr bool pop() {
        if (empty()) Traits::reportUnderflow("Called pop() on empty stack");
        bool result = top();
        size_t sz = size();
        ValueType data = bits & DataMask;
        data &= ~(ValueType(1) << (sz - 1));
        bits = packBits(sz - 1, data);
        return result;
    }

    /// \Returns the top element.
    /// \pre `!empty()`
    constexpr bool top() const {
        if (empty()) Traits::reportUnderflow("Called top() on empty stack");
        size_t sz = size();
        return (bits >> (sz - 1)) & 1;
    }

private:
    static constexpr size_t NumTotalBits = sizeof(ValueType) * CHAR_BIT;
    static constexpr size_t NumSizeBits = std::countr_zero(NumTotalBits);
    static constexpr size_t NumDataBits = NumTotalBits - NumSizeBits;
    static constexpr ValueType DataMask = (ValueType(1) << NumDataBits) - 1;

    constexpr static ValueType packBits(size_t size, ValueType data) {
        assert(size <= NumDataBits && "Invalid size");
        assert((data & ~DataMask) == 0 && "Invalid data");
        return (ValueType(size) << NumDataBits) | data;
    }

    ValueType bits = 0;
};

static_assert(TinyBoolStack<uint64_t>::capacity() == 58);
static_assert(TinyBoolStack<uint32_t>::capacity() == 27);
static_assert(TinyBoolStack<uint16_t>::capacity() == 12);
static_assert(TinyBoolStack<uint8_t>::capacity() == 5);

} // namespace sdb

#endif // SCATHADB_UTIL_TINYBOOLSTACK_H_
