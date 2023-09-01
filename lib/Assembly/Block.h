#ifndef SCATHA_ASM_BLOCK_H_
#define SCATHA_ASM_BLOCK_H_

#include <string>
#include <string_view>

#include <range/v3/range.hpp>
#include <utl/vector.hpp>

#include "Assembly/Instruction.h"
#include "Assembly/Value.h"

namespace scatha::Asm {

class Block {
public:
    using Iterator = utl::vector<Instruction>::iterator;
    using ConstIterator = utl::vector<Instruction>::const_iterator;

    explicit Block(size_t id, std::string name):
        Block(id, std::move(name), {}) {}

    explicit Block(size_t id,
                   std::string name,
                   std::initializer_list<Instruction> instructions):
        _id(id), _name(std::move(name)), instructions(instructions) {}

    size_t id() const { return _id; }

    /// \Returns `true` if the address of this block should appear in the
    /// global symbol table.
    bool isPublic() const { return _public; }

    void setPublic(bool value = true) { _public = value; }

    std::string_view name() const { return _name; }

    size_t instructionCount() const { return instructions.size(); }
    bool empty() const { return instructions.empty(); }

    auto begin() { return instructions.begin(); }
    auto begin() const { return instructions.begin(); }

    auto end() { return instructions.end(); }
    auto end() const { return instructions.end(); }

    void insertBack(Instruction const& instruction) {
        instructions.push_back(instruction);
    }
    void insert(ConstIterator position, Instruction const& instruction) {
        instructions.insert(position, instruction);
    }
    void insert(ConstIterator position, ranges::range auto&& instructions) {
        this->instructions.insert(position,
                                  ranges::begin(instructions),
                                  ranges::end(instructions));
    }

private:
    size_t _id   : 63;
    bool _public : 1 = false;
    std::string _name;
    utl::vector<Instruction> instructions;
};

} // namespace scatha::Asm

#endif // SCATHA_ASM_BLOCK_H_
