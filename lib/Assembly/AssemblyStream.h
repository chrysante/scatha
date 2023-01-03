#ifndef SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

#include <memory>
#include <list>

#include "Assembly/Value.h"
#include "Assembly/Instruction.h"

namespace scatha::Asm {
	
class AssemblyStream {
public:
    using Iterator = std::list<Instruction>::iterator;
    using ConstIterator = std::list<Instruction>::const_iterator;
    
    AssemblyStream() = default;
    
    Iterator      begin()         { return elems.begin(); }
    ConstIterator begin()   const { return elems.begin(); }
    Iterator      end()           { return elems.end(); }
    ConstIterator end()     const { return elems.end(); }
    Iterator      backItr()       { return std::prev(end()); }
    ConstIterator backItr() const { return std::prev(end()); }
    
    Iterator insert(ConstIterator pos, Instruction inst) {
        return elems.insert(pos, inst);
    }
    
    Iterator erase(ConstIterator pos) {
        return elems.erase(pos);
    }
    
    void add(Instruction inst) {
        elems.push_back(inst);
    }
    
private:
    std::list<Instruction> elems;
};
	
} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

