// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <list>
#include <memory>
#include <span>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

namespace scatha::Asm {

class Block;

class SCATHA_TESTAPI AssemblyStream {
public:
    AssemblyStream() = default;

    SCATHA_API AssemblyStream(AssemblyStream&&) noexcept;
    SCATHA_API AssemblyStream& operator=(AssemblyStream&&) noexcept;
    SCATHA_API ~AssemblyStream();

    auto begin() { return blocks.begin(); }
    auto begin() const { return blocks.begin(); }

    auto end() { return blocks.end(); }
    auto end() const { return blocks.end(); }

    Block* add(Block block);

    std::span<u8 const> dataSection() const { return data; }

    void setDataSection(utl::vector<u8> data) { this->data = std::move(data); }

private:
    std::list<Block> blocks;
    utl::vector<u8> data;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
