#include "IR/CFG/User.h"

#include <range/v3/algorithm.hpp>

using namespace scatha;
using namespace ir;

User::User(NodeType nodeType,
           Type const* type,
           std::string name,
           utl::small_vector<Value*> operands):
    Value(nodeType, type, std::move(name)) {
    setOperands(std::move(operands));
}

void User::setOperand(size_t index, Value* operand) {
    SC_ASSERT(index < _operands.size(),
              "`index` not valid for this instruction");
    if (auto* op = _operands[index]) {
        op->removeUserWeak(this);
    }
    if (operand) {
        operand->addUserWeak(this);
    }
    _operands[index] = operand;
}

void User::setOperands(utl::small_vector<Value*> operands) {
    clearOperands();
    _operands = std::move(operands);
    for (auto* op: _operands) {
        if (op) {
            op->addUserWeak(this);
        }
    }
}

void User::updateOperand(Value const* oldOperand, Value* newOperand) {
    bool leastOne = false;
    for (auto&& [index, op]: _operands | ranges::views::enumerate) {
        if (op == oldOperand) {
            setOperand(index, newOperand);
            leastOne = true;
        }
    }
    SC_ASSERT(leastOne, "Not found");
}

void User::removeOperand(size_t index) {
    auto const itr = _operands.begin() + index;
    (*itr)->removeUserWeak(this);
    _operands.erase(itr);
}

void User::clearOperands() {
    for (auto& op: _operands) {
        if (op) {
            op->removeUserWeak(this);
        }
        op = nullptr;
    }
}

bool User::directlyUses(Value const* value) const {
    return ranges::find(_operands, value) != ranges::end(_operands);
}
