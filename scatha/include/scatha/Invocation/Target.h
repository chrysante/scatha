#ifndef SCATHA_INVOCATION_TARGET_H_
#define SCATHA_INVOCATION_TARGET_H_

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Sema/Fwd.h>

namespace scatha {

/// Different types of targets the compiler can generate
/// - `Executable` Generates a binary program and makes the output file
/// executable on the system
/// - `BinaryOnly` Generates a binary program that can not be executed directly
/// - `StaticLibrary` Generates a static library
enum class TargetType { Executable, BinaryOnly, StaticLibrary };

/// Represents the result of a compiler invocation
class SCATHA_API Target {
public:
    /// Temporary type until we have a better static library representation
    struct StaticLib {
        /// Serialized symbol table
        std::string symbolTable;
        /// Serialized object code in IR representation
        std::string objectCode;
    };

    Target(Target&&) noexcept;
    Target& operator=(Target&&) noexcept;
    ~Target();

    /// \Returns the type of this target
    TargetType type() const { return _type; }

    /// \Returns the name of this target
    std::string const& name() const { return _name; }

    /// \Returns the programs symbol table
    /// This is valid for all target types
    sema::SymbolTable const& symbolTable() const { return *_sym; }

    /// \Returns the compiled binary data if available or empty span
    /// Only meaningful if `type()` is `Executable` or `BinaryOnly`
    std::span<uint8_t const> binary() const { return _binary; }

    /// \Returns the compiled debug info if available
    /// Only meaningful if `type()` is `Executable` or `BinaryOnly`
    std::string const& debugInfo() const { return _debugInfo; }

    /// \Returns the serialized static library
    /// Only meaningful if `type()` is `StaticLibrary`
    StaticLib const& staticLib() const { return _staticLib; }

    /// Writes this target to the destination directory \p dir
    void writeToDisk(std::filesystem::path const& dir) const;

private:
    friend class CompilerInvocation;

    /// Construct a binary target
    Target(TargetType type, std::string name,
           std::unique_ptr<sema::SymbolTable> sym, std::vector<uint8_t> binary,
           std::string debugInfo);

    /// Construct a library target
    Target(TargetType type, std::string name,
           std::unique_ptr<sema::SymbolTable> sym, StaticLib staticLib);

    TargetType _type;
    std::string _name;
    std::unique_ptr<sema::SymbolTable> _sym;

    /// Binary targets
    std::vector<uint8_t> _binary;
    std::string _debugInfo;

    /// Library targets
    StaticLib _staticLib;
};

} // namespace scatha

#endif // SCATHA_INVOCATION_TARGET_H_
