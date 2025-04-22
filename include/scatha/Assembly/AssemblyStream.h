#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <utl/hashtable.hpp>
#include <utl/ilist.hpp>

#include <scatha/Common/Base.h>
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
    utl::ilist<Block>::iterator begin();
    utl::ilist<Block>::const_iterator begin() const;
    utl::ilist<Block>::iterator end();
    utl::ilist<Block>::const_iterator end() const;
    /// @}

    /// Add the block \p block
    /// \Returns a pointer to the added block
    Block* add(std::unique_ptr<Block> block);

    /// \Returns a view over the data section
    std::span<u8 const> dataSection() const;

    ///
    void setDataSection(std::vector<u8> data);

    ///
    utl::hashmap<size_t, std::string> const& dataLabels() const;

    ///
    void setDataLabels(utl::hashmap<size_t, std::string> labels);

    /// \Returns a view over the jump sites
    std::span<Jumpsite const> jumpSites() const;

    ///
    void setJumpSites(std::vector<Jumpsite> data);

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
