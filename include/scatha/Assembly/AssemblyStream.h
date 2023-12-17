#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <iosfwd>
#include <list>
#include <memory>
#include <span>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/ForeignFunctionDecl.h>
#include <scatha/Common/Metadata.h>

namespace scatha::Asm {

class Block;
struct Jumpsite;

class SCATHA_API AssemblyStream: public ObjectWithMetadata {
public:
    /// Lifetime functions @{
    AssemblyStream();
    AssemblyStream(AssemblyStream const&) = delete;
    AssemblyStream& operator=(AssemblyStream const&) = delete;
    AssemblyStream(AssemblyStream&&) noexcept;
    AssemblyStream& operator=(AssemblyStream&&) noexcept;
    ~AssemblyStream();
    /// @}

    /// Range accessors @{
    std::list<Block>::iterator begin();
    std::list<Block>::const_iterator begin() const;
    std::list<Block>::iterator end();
    std::list<Block>::const_iterator end() const;
    /// @}

    /// Add the block \p block
    /// \Returns a pointer to the added block
    Block* add(Block block);

    /// \Returns a view over the data section
    std::span<u8 const> dataSection() const;

    ///
    void setDataSection(std::vector<u8> data);

    /// \Returns a view over the jump sites
    std::span<Jumpsite const> jumpSites() const;

    ///
    void setJumpSites(std::vector<Jumpsite> data);

    /// List of foreign library names to be imported by the VM
    std::span<std::string const> foreignLibraries() const;

    ///
    void setForeignLibraries(std::vector<std::string> libs);

    /// \Returns a view over the foreign function declarations
    std::span<ForeignFunctionDecl const> foreignFunctions() const;

    ///
    void setForeignFunctions(std::vector<ForeignFunctionDecl> functions);

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
};

/// Prints \p assemblyStream to `std::cout`
SCATHA_API void print(AssemblyStream const& assemblyStream);

/// Prints \p assemblyStream to \p ostream
SCATHA_API void print(AssemblyStream const& assemblyStream,
                      std::ostream& ostream);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
