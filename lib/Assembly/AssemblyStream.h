// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

#include <list>
#include <memory>

#include <scatha/Basic/Basic.h>

namespace scatha::Asm {

class Instruction;

class AssemblyStream {
public:
    using Iterator      = std::list<Instruction>::iterator;
    using ConstIterator = std::list<Instruction>::const_iterator;

    AssemblyStream() = default;

    SCATHA(API) AssemblyStream(AssemblyStream&&) noexcept;
    SCATHA(API) AssemblyStream& operator=(AssemblyStream&&) noexcept;
    SCATHA(API) ~AssemblyStream();
    
    Iterator begin() { return elems.begin(); }
    ConstIterator begin() const { return elems.begin(); }
    Iterator end() { return elems.end(); }
    ConstIterator end() const { return elems.end(); }
    Iterator backItr() { return std::prev(end()); }
    ConstIterator backItr() const { return std::prev(end()); }

    Iterator insert(ConstIterator before, Instruction inst);

    Iterator erase(ConstIterator pos);

    void add(Instruction inst);

private:
    std::list<Instruction> elems;
};

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_
