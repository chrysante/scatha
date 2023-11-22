#include "Runtime/Executor.h"

using namespace scatha;

std::unique_ptr<Executor> Executor::Make() {
    return std::unique_ptr<Executor>(new Executor());
}

std::unique_ptr<Executor> Executor::Make(Program const& program) {
    return std::unique_ptr<Executor>(new Executor(program));
}

void Executor::load(Program const& program) {
    prog = program;
    vm.loadBinary(prog.data());
}

void Executor::addFunction(FuncDecl decl, InternalFuncPtr impl, void* userptr) {
    vm.setFunction(decl.slot, decl.index, { decl.name, impl, userptr });
}

Executor::Executor(Program const& prog) { load(prog); }
