#include "Assembly/AssemblyStream.h"

#include "Assembly/Block.h"

using namespace scatha;
using namespace Asm;

struct AssemblyStream::Impl {
    std::list<Block> blocks;
    std::vector<u8> data;
    std::vector<Jumpsite> jumpsites;
};

AssemblyStream::AssemblyStream(): impl(std::make_unique<Impl>()) {}

AssemblyStream::AssemblyStream(AssemblyStream&&) noexcept = default;

AssemblyStream& AssemblyStream::operator=(AssemblyStream&&) noexcept = default;

AssemblyStream::~AssemblyStream() = default;

std::list<Block>::iterator AssemblyStream::begin() {
    return impl->blocks.begin();
}
std::list<Block>::const_iterator AssemblyStream::begin() const {
    return impl->blocks.begin();
}

std::list<Block>::iterator AssemblyStream::end() { return impl->blocks.end(); }
std::list<Block>::const_iterator AssemblyStream::end() const {
    return impl->blocks.end();
}

Block* AssemblyStream::add(Block block) {
    impl->blocks.push_back(std::move(block));
    return &impl->blocks.back();
}

std::span<u8 const> AssemblyStream::dataSection() const { return impl->data; }

void AssemblyStream::setDataSection(std::vector<u8> data) {
    impl->data = std::move(data);
}

void AssemblyStream::setJumpSites(std::vector<Jumpsite> jumpsites) {
    impl->jumpsites = std::move(jumpsites);
}

std::span<Jumpsite const> AssemblyStream::jumpSites() const {
    return impl->jumpsites;
}
