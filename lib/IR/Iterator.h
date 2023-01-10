#ifndef SCATHA_IR_ITERATOR_H_
#define SCATHA_IR_ITERATOR_H_

#include "IR/Common.h"
#include "IR/List.h"

namespace scatha::ir::internal {

/// Iterator to walk across instructions in a list of basic blocks
template <typename BBItr, typename InstItr>
class InstructionIteratorImpl {
public:
    using difference_type   = typename std::iterator_traits<InstItr>::difference_type;
    using value_type        = typename std::iterator_traits<InstItr>::value_type;
    using pointer           = typename std::iterator_traits<InstItr>::pointer;
    using reference         = typename std::iterator_traits<InstItr>::reference;
    using iterator_category = typename std::iterator_traits<InstItr>::iterator_category;
    
    using BasicBlockIterator = BBItr;
    using InstructionIterator = InstItr;
    
    InstructionIteratorImpl(BBItr bbItr, InstItr instItr):
        bbItr(bbItr), instItr(instItr) {}
    
    template <std::convertible_to<BBItr> BBI2, std::convertible_to<InstItr> II2>
    InstructionIteratorImpl(InstructionIteratorImpl<BBI2, II2> const& rhs):
        bbItr(rhs.bbItr), instItr(rhs.instItr) {}
    
    InstructionIteratorImpl& operator=(std::convertible_to<InstItr> auto const& rhsInstItr)& {
        instItr = rhsInstItr;
        handleBBBoundary();
        return *this;
    }
    
    auto* toAddress() const { return instItr.to_address(); }
    
    BBItr basicBlockIterator() const { return bbItr; }
    
    InstItr instructionIterator() const { return instItr; }
    
    auto& basicBlock() const { return *basicBlockIterator(); }
    
    auto& instruction() const { return *instructionIterator(); }
    
    auto* operator->() const { return toAddress(); }
    
    auto& operator*() const { return *toAddress(); }

    InstructionIteratorImpl& operator++() {
        ++instItr;
        handleBBBoundary();
        return *this;
    }
    
    InstructionIteratorImpl operator++(int) {
        auto result = *this;
        ++*this;
        return result;
    }
    
    InstructionIteratorImpl& operator--() {
        --instItr;
        if (instItr == bbItr->end()) { // This works because ilist is actually a circular list with one sentinel which is the end() node.
            --bbItr;
            instItr = std::prev(bbItr->end());
        }
        return *this;
    }
    
    InstructionIteratorImpl operator--(int) {
        auto result = *this;
        --*this;
        return result;
    }
    
    bool operator==(InstructionIteratorImpl const& rhs) const = default;
    
    template <std::equality_comparable_with<BBItr> BBI2, std::equality_comparable_with<InstItr> II2>
    bool operator==(InstructionIteratorImpl<BBI2, II2> const& rhs) const {
        return bbItr == rhs.bbItr && instItr == rhs.instItr;
    }
    
private:
    void handleBBBoundary() {
        if (instItr != bbItr->instructions.end()) {
            return;
        }
        BBItr const next = std::next(bbItr);
        bool const isEnd = next == bbItr->parent()->basicBlocks().end();
        bbItr = next;
        instItr = isEnd ? InstItr{} : bbItr->instructions.begin();
    }
    
private:
    BBItr bbItr;
    InstItr instItr;
};

} // namespace scatha::ir::internal

namespace scatha::ir {

} // namespace scatha::ir

#endif // SCATHA_IR_ITERATOR_H_

