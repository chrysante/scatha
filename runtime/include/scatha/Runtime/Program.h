#ifndef SCATHA_RUNTIME_PROGRAM_H_
#define SCATHA_RUNTIME_PROGRAM_H_

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <scatha/Runtime/Common.h>

namespace scatha {

class Program {
public:
    explicit Program(std::unordered_map<std::string, size_t> sym,
                     std::vector<uint8_t> bin):
        sym(std::move(sym)), bin(std::move(bin)) {}

    /// Find address of function \p name with argument types \p argTypes
    std::optional<size_t> findAddress(std::string_view name,
                                      std::span<QualType const> argTypes) const;

    /// \overload
    std::optional<size_t> findAddress(
        std::string_view name, std::initializer_list<QualType> argTypes) const {
        return findAddress(name, std::span<QualType const>(argTypes));
    }

    /// The binary data representation of the program
    uint8_t const* binary() const { return bin.data(); }

private:
    std::unordered_map<std::string, size_t> sym;
    std::vector<uint8_t> bin;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_PROGRAM_H_
