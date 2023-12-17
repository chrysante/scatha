#include "IR/Module.h"

#include "IR/CFG.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

Module::Module() = default;

Module::Module(Module&& rhs) noexcept = default;

Module& Module::operator=(Module&& rhs) noexcept = default;

Module::~Module() = default;

StructType const* Module::addStructure(UniquePtr<StructType> structure) {
    auto* result = structure.get();
    structs.push_back(std::move(structure));
    return result;
}

Global* Module::addGlobal(UniquePtr<Global> value) {
    auto* result = value.get();
    // clang-format off
    SC_MATCH (*value) {
        [&](Function& func) {
            value.release();
            func.set_parent(this);
            funcs.push_back(&func);
        },
        [&](Global& global) {
            if (auto* func = dyncast<ForeignFunction*>(&global)) {
                _extFunctions.insert(func);
            }
            value.release();
            global.set_parent(this);
            _globals.push_back(&global);
        },
    }; // clang-format on
    return result;
}

void Module::erase(Global* global) {
    // clang-format off
    SC_MATCH (*global) {
        [&](Function& function) {
            funcs.erase(&function);
        },
        [&](ForeignFunction& function) {
            _extFunctions.erase(&function);
            _globals.erase(&function);
        },
        [&](Global& global) {
            _globals.erase(&global);
        },
    }; // clang-format on
}

List<Function>::iterator Module::erase(List<Function>::const_iterator itr) {
    return funcs.erase(itr);
}

List<Function>::iterator Module::begin() { return funcs.begin(); }

List<Function>::const_iterator Module::begin() const { return funcs.begin(); }

List<Function>::iterator Module::end() { return funcs.end(); }

List<Function>::const_iterator Module::end() const { return funcs.end(); }

bool Module::empty() const { return funcs.empty(); }

Function& Module::front() { return funcs.front(); }

Function const& Module::front() const { return funcs.front(); }

Function& Module::back() { return funcs.back(); }

Function const& Module::back() const { return funcs.back(); }
