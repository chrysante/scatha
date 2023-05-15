// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <list>
#include <memory>
#include <span>
#include <vector>

#include <scatha/Common/Base.h>

namespace scatha::Asm {

class Block;

class SCATHA_TESTAPI AssemblyStream {
public:
    AssemblyStream();

    AssemblyStream(AssemblyStream const&)            = delete;
    AssemblyStream& operator=(AssemblyStream const&) = delete;
    SCATHA_API AssemblyStream(AssemblyStream&&) noexcept;
    SCATHA_API AssemblyStream& operator=(AssemblyStream&&) noexcept;
    SCATHA_API ~AssemblyStream();

    std::list<Block>::iterator begin();
    std::list<Block>::const_iterator begin() const;

    std::list<Block>::iterator end();
    std::list<Block>::const_iterator end() const;

    Block* add(Block block);

    std::span<u8 const> dataSection() const;

    void setDataSection(std::vector<u8> data);

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
