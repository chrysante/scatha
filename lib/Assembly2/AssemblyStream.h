#ifndef SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

#include <memory>

#include <utl/vector.hpp>

#include "Assembly2/Value.h"
#include "Assembly2/Instruction.h"

namespace scatha::asm2 {
	
class AssemblyStream {
    using Vector = utl::vector<Instruction>;
    template <typename T>
    using ItrBase = std::conditional_t<std::is_const_v<T>,
                                       Vector::const_iterator,
                                       Vector::iterator>;
    template <typename T>
    struct IteratorImpl: private ItrBase<T> {
    private:
        friend class AssemblyStream;
        IteratorImpl(ItrBase<T> itr): ItrBase<T>(itr) {}
        
    public:
        using ItrBase<T>::operator++;
        using ItrBase<T>::operator--;
        using ItrBase<T>::operator*;
        using ItrBase<T>::operator->;
        bool operator<=>(IteratorImpl const& rhs) const = default;
    };
    
public:
    using Iterator = IteratorImpl<Value>;
    using ConstIterator = IteratorImpl<Value const>;
    
    AssemblyStream() = default;
    
    Iterator begin() { return elems.begin(); }
    ConstIterator begin() const { return elems.begin(); }
    Iterator end() { return elems.end(); }
    ConstIterator end() const { return elems.end(); }
    
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
    Vector elems;
};
	
} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

