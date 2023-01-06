#include "Assembly/AssemblyStream.h"

#include "Assembly/Instruction.h"

using namespace scatha;
using namespace Asm;

AssemblyStream::AssemblyStream(AssemblyStream&&) noexcept = default;

AssemblyStream& AssemblyStream::operator=(AssemblyStream&&) noexcept = default;

AssemblyStream::~AssemblyStream() = default;

AssemblyStream::Iterator AssemblyStream::insert(ConstIterator before, Instruction inst) {
    return elems.insert(before, inst);
}

AssemblyStream::Iterator AssemblyStream::erase(ConstIterator pos) {
    return elems.erase(pos);
}

void AssemblyStream::add(Instruction inst) {
    elems.push_back(inst);
}
