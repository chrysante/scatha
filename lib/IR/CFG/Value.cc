#include "IR/CFG/Value.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

Value::~Value() { removeAllUses(); }

void Value::removeAllUses() {
    for (auto [user, count]: _users) {
        user->updateOperand(this, nullptr);
    }
    _users.clear();
}

void Value::addUserWeak(User* user) {
    auto const [itr, success] = _users.insert({ user, 0 });
    ++itr->second;
}

void Value::removeUserWeak(User* user) {
    auto itr = _users.find(user);
    SC_ASSERT(itr != _users.end(), "`user` is not a user of this value.");
    if (--itr->second == 0) {
        _users.erase(itr);
    }
}

void Value::setName(std::string name) {
    auto makeUnique = [&](std::string& name, Function& func) {
        func.nameFac.tryErase(_name);
        name = func.nameFac.makeUnique(std::move(name));
    };
    // clang-format off
    visit(*this, utl::overload{
        [&](BasicBlock& bb) {
            auto* func = bb.parent();
            if (func) {
                makeUnique(name, *func);
            }
        },
        [&](Instruction& inst) {
            auto* bb   = inst.parent();
            auto* func = bb ? bb->parent() : nullptr;
            if (func) {
                makeUnique(name, *func);
            }
        },
        [](Value const&) {}
    }); // clang-format on
    _name = std::move(name);
}

void Value::uniqueExistingName(Function& func) {
    _name = func.nameFac.makeUnique(std::move(_name));
}

void ir::privateDelete(Value* value) {
    visit(*value, [](auto& derived) { delete &derived; });
}

void ir::privateDestroy(Value* value) {
    visit(*value, [](auto& derived) { std::destroy_at(&derived); });
}
