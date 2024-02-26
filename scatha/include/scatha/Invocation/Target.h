#ifndef SCATHA_INVOCATION_TARGET_H_
#define SCATHA_INVOCATION_TARGET_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Base.h>

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
    /// \Returns the type of this target
    TargetType type() const { return _type; }

    /// \Returns the name of this target
    std::string const& name() const { return _name; }

    /// \Returns the compiled binary data if available or empty span
    /// Only meaningful if `type()` is `Executable` or `BinaryOnly`
    std::span<uint8_t const> binary() const { return _binary; }

    /// \Returns the compiled debug info if available
    /// Only meaningful if `type()` is `Executable` or `BinaryOnly`
    std::string const& debugInfo() const { return _debugInfo; }

    /// \Returns the serialized symbol table if available
    /// Only meaningful if `type()` is `StaticLibrary`
    std::string const& symbolTable() const { return _symbolTable; }

    /// \Returns the serialized object code if available
    /// Only meaningful if `type()` is `StaticLibrary`
    std::string const& objectCode() const { return _objectCode; }

    /// Writes this target to the destination directory \p dir
    void writeToDisk(std::filesystem::path const& dir) const;

private:
    friend class CompilerInvocation;

    /// Construct a binary target
    Target(TargetType type, std::string name, std::vector<uint8_t> binary,
           std::string debugInfo):
        _type(type),
        _name(std::move(name)),
        _binary(std::move(binary)),
        _debugInfo(std::move(debugInfo)) {}

    /// Construct a library target
    Target(TargetType type, std::string name, std::string symbolTable,
           std::string objectCode):
        _type(type),
        _name(std::move(name)),
        _symbolTable(std::move(symbolTable)),
        _objectCode(std::move(objectCode)) {}

    TargetType _type;
    std::string _name;

    /// Binary targets
    std::vector<uint8_t> _binary;
    std::string _debugInfo;

    /// Library targets
    std::string _symbolTable;
    std::string _objectCode;
};

} // namespace scatha

#endif // SCATHA_INVOCATION_TARGET_H_
