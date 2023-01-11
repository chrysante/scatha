#include "Assembly/AssemblyStream.h"

#include "Assembly/Block.h"

using namespace scatha;
using namespace Asm;

AssemblyStream::AssemblyStream(AssemblyStream&&) noexcept = default;

AssemblyStream& AssemblyStream::operator=(AssemblyStream&&) noexcept = default;

AssemblyStream::~AssemblyStream() = default;

Block* AssemblyStream::add(Block block) {
    blocks.push_back(std::move(block));
    return &blocks.back();
}
