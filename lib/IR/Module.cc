#include "IR/Module.h"

#include "IR/CFG.h"
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

ExtFunction* Module::extFunction(size_t slot, size_t index) {
    auto itr = _extFunctions.find({ slot, index });
    return itr != _extFunctions.end() ? itr->second : nullptr;
}

ExtFunction* Module::builtinFunction(svm::Builtin builtin) {
    return extFunction(svm::BuiltinFunctionSlot, static_cast<size_t>(builtin));
}

void Module::addStructure(UniquePtr<StructType> structure) {
    structs.push_back(std::move(structure));
}

void Module::addGlobal(UniquePtr<Value> value) {
    if (auto* func = dyncast<ExtFunction*>(value.get())) {
        _extFunctions[{ func->slot(), func->index() }] = func;
    }
    _globals.push_back(std::move(value));
}

void Module::addConstantData(UniquePtr<ConstantData> value) {
    _constantData.push_back(std::move(value));
}

void Module::eraseFunction(Function* function) {
    eraseFunction(List<Function>::iterator(function));
}

List<Function>::iterator Module::eraseFunction(
    List<Function>::const_iterator itr) {
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
