#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <iosfwd>
#include <list>
#include <memory>
#include <span>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/Metadata.h>

namespace scatha::Asm {

class Block;
struct Jumpsite;

class SCATHA_API AssemblyStream: public ObjectWithMetadata {
public:
    AssemblyStream();

    AssemblyStream(AssemblyStream const&) = delete;
    AssemblyStream& operator=(AssemblyStream const&) = delete;
    AssemblyStream(AssemblyStream&&) noexcept;
    AssemblyStream& operator=(AssemblyStream&&) noexcept;
    ~AssemblyStream();

    std::list<Block>::iterator begin();
    std::list<Block>::const_iterator begin() const;

    std::list<Block>::iterator end();
    std::list<Block>::const_iterator end() const;

    Block* add(Block block);

    std::span<u8 const> dataSection() const;

    void setDataSection(std::vector<u8> data);

    void setJumpSites(std::vector<Jumpsite> data);

    std::span<Jumpsite const> jumpSites() const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
};

SCATHA_API void print(AssemblyStream const& assemblyStream);

SCATHA_API void print(AssemblyStream const& assemblyStream,
                      std::ostream& ostream);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
