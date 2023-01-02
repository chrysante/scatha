#ifndef SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_
#define SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

#include <memory>

#include <utl/vector.hpp>

#include "Assembly2/Elements.h"

namespace scatha::asm2 {
	
class AssemblyStream {
    using Vector = utl::vector<std::unique_ptr<Element>>;
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
    using Iterator = IteratorImpl<Element>;
    using ConstIterator = IteratorImpl<Element const>;
    
    AssemblyStream() = default;
    
    Iterator begin() { return elems.begin(); }
    ConstIterator begin() const { return elems.begin(); }
    Iterator end() { return elems.end(); }
    ConstIterator end() const { return elems.end(); }
    
    Iterator insert(ConstIterator pos, std::unique_ptr<Element> elem) {
        return elems.insert(pos, std::move(elem));
    }
    
    Iterator erase(ConstIterator pos) {
        return elems.erase(pos);
    }
    
    void add(std::unique_ptr<Element> elem) {
        elems.push_back(std::move(elem));
    }
    
private:
    Vector elems;
};
	
} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_ASSEMBLYSTREAM_H_

