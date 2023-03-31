#include "IR/Module.h"

#include "IR/CFG/Function.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

void Module::addFunction(Function* function) {
    function->set_parent(this);
    funcs.push_back(function);
}

void Module::addFunction(UniquePtr<Function> function) {
    /// `.release()` because `List<>` takes ownership.
    addFunction(function.release());
}

Module::Module(Module&& rhs) noexcept = default;

Module& Module::operator=(Module&& rhs) noexcept = default;

Module::~Module() = default;

void Module::addStructure(UniquePtr<StructureType> structure) {
    structs.push_back(std::move(structure));
}

void Module::addGlobal(UniquePtr<Value> value) {
    _globals.push_back(std::move(value));
}

void Module::eraseFunction(Function* function) { funcs.erase(function); }

void Module::eraseFunction(List<Function>::iterator itr) {
    eraseFunction(itr.to_address());
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
