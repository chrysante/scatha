#include "Runtime/Executor.h"

#include "Runtime/LibSupport.h"

using namespace scatha;

std::unique_ptr<Executor> Executor::Make() {
    return std::unique_ptr<Executor>(new Executor());
}

std::unique_ptr<Executor> Executor::Make(Program program) {
    return std::unique_ptr<Executor>(new Executor(std::move(program)));
}

void Executor::load(Program program) {
    prog = std::move(program);
    vm.loadBinary(prog.data());
}

void Executor::addFunction(FuncDecl decl, InternalFuncPtr impl, void* userptr) {
    defineFunction(vm, decl.address, std::move(decl).name, impl, userptr);
}

Executor::Executor(Program prog) { load(std::move(prog)); }
