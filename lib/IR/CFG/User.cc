#include "IR/CFG/User.h"

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

using namespace scatha;
using namespace ir;

User::~User() { clearOperands(); }

User::User(NodeType nodeType,
           Type const* type,
           std::string name,
           utl::small_vector<Value*> operands):
    Value(nodeType, type, std::move(name)) {
    setOperands(std::move(operands));
}

std::optional<size_t> User::indexOf(Value const* operand) const {
    auto itr = ranges::find(_operands, operand);
    if (itr != _operands.end()) {
        return utl::narrow_cast<size_t>(itr - _operands.begin());
    }
    return std::nullopt;
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
    [[maybe_unused]] bool result = tryUpdateOperand(oldOperand, newOperand);
    SC_ASSERT(result, "Not found");
}

bool User::tryUpdateOperand(Value const* oldOperand, Value* newOperand) {
    bool result = false;
    for (auto&& [index, op]: _operands | ranges::views::enumerate) {
        if (op == oldOperand) {
            setOperand(index, newOperand);
            result = true;
        }
    }
    return result;
}

void User::addOperand(Value* op) {
    _operands.push_back(op);
    op->addUserWeak(this);
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
