// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_

#include <list>
#include <memory>

#include <scatha/Basic/Basic.h>

namespace scatha::Asm {

class Block;

class SCATHA(TEST_API) AssemblyStream {
public:
    AssemblyStream() = default;

    SCATHA(API) AssemblyStream(AssemblyStream&&) noexcept;
    SCATHA(API) AssemblyStream& operator=(AssemblyStream&&) noexcept;
    SCATHA(API) ~AssemblyStream();

    auto begin() { return blocks.begin(); }
    auto begin() const { return blocks.begin(); }

    auto end() { return blocks.end(); }
    auto end() const { return blocks.end(); }

    Block* add(Block block);

private:
    std::list<Block> blocks;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLYSTREAM_H_
