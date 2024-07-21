#include "IR/CFG/Value.h"

#include "Common/Ranges.h"
#include "IR/Attributes.h"
#include "IR/CFG.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"
#include "IR/ValueRef.h"

using namespace scatha;
using namespace ir;

Value::Value(NodeType nodeType, Type const* type, std::string name) noexcept:
    _nodeType(nodeType), _type(type), _name(std::move(name)) {}

Value::~Value() {
    removeAllUses();
    clearAllReferences();
}

void Value::removeAllUses() {
    /// We make a copy of the user list because `updateOperand()` modifies the
    /// list
    for (auto* user: users() | ToSmallVector<>) {
        user->updateOperand(this, nullptr);
    }
    SC_ASSERT(_users.empty(),
              "The calls to updateOperand() should have cleared this");
}

void Value::replaceAllUsesWith(Value* newValue) {
    if (this == newValue) {
        return;
    }
    /// We store the user list in a temporary vector because in the loop body
    /// the user is erased from the user list and iterators would be
    /// invalidated.
    for (auto* user: users() | ToSmallVector<>) {
        user->updateOperand(this, newValue);
    }
}

void Value::clearAllReferences() {
    for (auto* ref: _references) {
        ref->_value = nullptr;
    }
    _references.clear();
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
    if (auto* BB = dyncast<BasicBlock*>(this)) {
        auto* func = BB->parent();
        if (func) {
            makeUnique(name, *func);
        }
    }
    else if (auto* inst = dyncast<Instruction*>(this)) {
        auto* BB = inst->parent();
        auto* func = BB ? BB->parent() : nullptr;
        if (func) {
            makeUnique(name, *func);
        }
    }
    _name = std::move(name);
}

void Value::uniqueExistingName(Function& func) {
    _name = func.nameFac.makeUnique(std::move(_name));
}

void ir::do_delete(Value& value) {
    visit(value, [](auto& derived) { delete &derived; });
}

void ir::do_destroy(Value& value) {
    visit(value, [](auto& derived) { std::destroy_at(&derived); });
}

void Value::setPointerInfo(PointerInfoDesc desc) {
    ptrInfo = std::make_unique<PointerInfo>(desc);
}

Attribute const* Value::addAttribute(UniquePtr<Attribute> attrib) {
    auto [itr, success] = _attribs.emplace(attrib->type(), std::move(attrib));
    SC_ASSERT(success, "Attribute already present");
    return itr->second.get();
}

void Value::removeAttribute(AttributeType attribType) {
    _attribs.erase(attribType);
}

Attribute const* Value::get(AttributeType attrib) const {
    auto itr = _attribs.find(attrib);
    if (itr != _attribs.end()) {
        return itr->second.get();
    }
    return nullptr;
}
