#include "CodeGen/CodeGen2/AddressDescriptor.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace cg;
using namespace ir;

Asm::Value Address::get() const {
    SC_ASSERT(!empty(), "Can't get an empty address");
    if (lit) {
        return *lit;
    }
    if (reg) {
        return *reg;
    }
    if (mem) {
        return *mem;
    }
    SC_UNREACHABLE();
}

bool Address::empty() const { return !lit && !reg && !mem; }

void UserMap::addInstruction(ir::Instruction const* inst) {
    auto success =
        map.insert({ inst, inst->countedUsers() | ranges::to<UserSet> }).second;
    SC_ASSERT(success, "Instruction has been added before");
}

bool UserMap::removeUser(ir::Instruction const* inst,
                         ir::Instruction const* user) {
    SC_ASSERT(map.contains(inst),
              "inst must be registered before calling this");
    //    auto& users = map[instOp];
    //    SC_ASSERT(!users.empty(), "This operand must have at least one user,
    //    i.e. `inst`"); auto const numRemoved = users.erase(inst);
    //    SC_ASSERT(numRemoved == 1, "");
}

ResolveResult AddressDescriptor::resolve(ir::Instruction const* inst) {
    ResolveResult res;
    auto success =
        userMap
            .insert({ inst,
                      inst->users() |
                          ranges::to<utl::hashset<Instruction const*>> })
            .second;
    SC_ASSERT(success,
              "Has resolve() been called before for this instruction?");
    utl::small_vector<Instruction const*> nowEmptyUsers;
    for (auto* op: inst->operands()) {
        auto* address = find(op);
        if (auto* instOp = dyncast<Instruction const*>(op)) {
            auto& users = userMap[instOp];
            SC_ASSERT(!users.empty(),
                      "This operand must have at least one user, i.e. `inst`");
            auto const numRemoved = users.erase(inst);
            SC_ASSERT(numRemoved == 1, "");
            if (users.empty()) {
                //                nowEmptyUsers.push_back(op)
            }
        }
        res.operands.push_back(address->get());
    }
}

Address* AddressDescriptor::find(ir::Value const* value) {
    auto itr = addressMap.find(value);
    if (itr != addressMap.end()) {
        return itr->second;
    }
}
