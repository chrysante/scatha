#ifndef SCATHA_IR_ITERATOR_H_
#define SCATHA_IR_ITERATOR_H_

#include "IR/Common.h"
#include "IR/List.h"

namespace scatha::ir::internal {

/// Iterator to walk across instructions in a list of basic blocks
template <typename BBItr, typename InstItr>
class InstructionIteratorImpl {
    template <typename, typename>
    friend class InstructionIteratorImpl;

public:
    using value_type = typename std::iterator_traits<InstItr>::value_type;
    using difference_type =
        typename std::iterator_traits<InstItr>::difference_type;
    using pointer   = typename std::iterator_traits<InstItr>::pointer;
    using reference = typename std::iterator_traits<InstItr>::reference;
    using iterator_category =
        typename std::iterator_traits<InstItr>::iterator_category;

    using BasicBlockIterator  = BBItr;
    using InstructionIterator = InstItr;

    InstructionIteratorImpl() = default;

    InstructionIteratorImpl(BBItr bbItr, InstItr instItr):
        bbItr(bbItr), instItr(instItr) {
        handleBBBoundary();
    }

    template <std::convertible_to<BBItr> BBI2, std::convertible_to<InstItr> II2>
    InstructionIteratorImpl(InstructionIteratorImpl<BBI2, II2> const& rhs):
        bbItr(rhs.bbItr), instItr(rhs.instItr) {}

    InstructionIteratorImpl& operator=(
        std::convertible_to<InstItr> auto const& rhsInstItr) & {
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

    bool operator==(InstructionIteratorImpl const& rhs) const {
        return cmpImpl(*this, rhs);
    }

    template <std::equality_comparable_with<BBItr> BBI2,
              std::equality_comparable_with<InstItr> II2>
    bool operator==(InstructionIteratorImpl<BBI2, II2> const& rhs) const {
        return cmpImpl(*this, rhs);
    }

private:
    static bool cmpImpl(auto const& lhs, auto const& rhs) {
        return lhs.bbItr == rhs.bbItr && lhs.instItr == rhs.instItr;
    }

    void handleBBBoundary() {
        while (instItr == bbItr->end()) {
            BBItr const next = std::next(bbItr);
            bool const isEnd = next == bbItr->parent()->end();
            bbItr            = next;
            instItr          = isEnd ? InstItr{} : bbItr->begin();
        }
    }

private:
    BBItr bbItr;
    InstItr instItr;
};

struct PhiSentinel {};

template <bool IsConst, typename BB = BasicBlock>
struct PhiIteratorImpl {
    using Itr = std::conditional_t<IsConst,
                                   typename BB::ConstIterator,
                                   typename BB::Iterator>;

public:
    using value_type      = std::conditional_t<IsConst, Phi const, Phi>;
    using difference_type = typename std::iterator_traits<Itr>::difference_type;
    using pointer         = value_type*;
    using reference       = value_type&;
    using iterator_category =
        typename std::iterator_traits<Itr>::iterator_category;

    PhiIteratorImpl(Itr begin, Itr end): itr(begin), end(end) {}

    PhiIteratorImpl& operator=(Itr begin) {
        itr = begin;
        return *this;
    }

    operator Itr() const { return itr; }

    operator typename BB::ConstIterator() const
        requires(!IsConst)
    {
        return itr;
    }

    pointer toAddress() const { return cast<pointer>(itr.to_address()); }

    reference operator*() const { return *toAddress(); }

    pointer operator->() const { return toAddress(); }

    PhiIteratorImpl& operator++() {
        ++itr;
        return *this;
    }

    PhiIteratorImpl operator++(int) {
        auto result = *this;
        ++*this;
        return result;
    }

    bool operator==(PhiSentinel const&) const {
        return itr == end || !isa<Phi>(*itr);
    }

private:
    Itr itr, end;
};

} // namespace scatha::ir::internal

namespace scatha::ir {} // namespace scatha::ir

#endif // SCATHA_IR_ITERATOR_H_
