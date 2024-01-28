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

    explicit Block(LabelID id, std::string name):
        Block(id, std::move(name), {}) {}

    explicit Block(LabelID id, std::string name,
                   std::initializer_list<Instruction> instructions):
        _id(id), _name(std::move(name)), instructions(instructions) {}

    LabelID id() const { return _id; }

    /// \Returns `true` if the address of this block should appear in the
    /// global symbol table.
    bool isExternallyVisible() const { return _extern; }

    void setExternallyVisible(bool value = true) { _extern = value; }

    std::string_view name() const { return _name; }

    size_t instructionCount() const { return instructions.size(); }
    bool empty() const { return instructions.empty(); }

    auto begin() { return instructions.begin(); }
    auto begin() const { return instructions.begin(); }

    auto end() { return instructions.end(); }
    auto end() const { return instructions.end(); }

    auto& back() { return instructions.back(); }
    auto& back() const { return instructions.back(); }

    void insertBack(Instruction const& instruction) {
        instructions.push_back(instruction);
    }
    void insert(ConstIterator position, Instruction const& instruction) {
        instructions.insert(position, instruction);
    }
    void insert(ConstIterator position, ranges::range auto&& instructions) {
        this->instructions.insert(position, ranges::begin(instructions),
                                  ranges::end(instructions));
    }

private:
    LabelID _id  : 63;
    bool _extern : 1 = false;
    std::string _name;
    utl::vector<Instruction> instructions;
};

} // namespace scatha::Asm

#endif // SCATHA_ASM_BLOCK_H_
