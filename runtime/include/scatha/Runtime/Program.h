#ifndef SCATHA_RUNTIME_PROGRAM_H_
#define SCATHA_RUNTIME_PROGRAM_H_

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <scatha/Runtime/Common.h>

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha {

class Program {
public:
    explicit Program(std::vector<uint8_t> bin,
                     std::unique_ptr<sema::SymbolTable> sym);

    Program(Program&&) noexcept;

    Program& operator=(Program&&) noexcept;

    ~Program();

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

    /// The symbol table
    sema::SymbolTable const& symbolTable() const { return *sym; }

private:
    std::vector<uint8_t> bin;
    std::unique_ptr<sema::SymbolTable> sym;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_PROGRAM_H_
