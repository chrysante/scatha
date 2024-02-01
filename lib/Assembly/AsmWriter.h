#ifndef SCATHA_ASSEMBLY_ASMWRITER
#define SCATHA_ASSEMBLY_ASMWRITER

#include <string_view>
#include <vector>

#include <utl/utility.hpp>

#include "Common/Base.h"
#include "Common/Utility.h"

namespace scatha::Asm {

/// Common base class of `Assembler` and `Linker` that implements methods to
/// write executable binaries
struct AsmWriter {
    explicit AsmWriter(std::vector<u8>& binary, size_t currentPosition = 0):
        binary(binary), pos(currentPosition) {}

    /// Writes  \p value into the stream at the current position
    template <typename T>
    void put(u64 value) {
        for (auto byte: decompose(utl::narrow_cast<T>(value))) {
            binary.insert(binary.begin() + static_cast<ssize_t>(position()),
                          byte);
            ++pos;
        }
    }

    /// Write  \p text and a null terminator into the stream at the current
    /// position
    void putNullTerm(std::string_view text) {
        for (auto c: text) {
            put<char>(static_cast<u64>(c));
        }
        put<char>(0);
    }

    /// Writes \p numBytes of all ones into the stream at the current position
    void putPlaceholderBytes(size_t numBytes) {
        for (size_t i = 0; i < numBytes; ++i) {
            put<u8>(0xFF);
        }
    }

    ///
    size_t position() const { return pos; }

    ///
    void setPosition(size_t position) { pos = position; }

    std::vector<u8>& binary;
    size_t pos = 0;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASMWRITER
