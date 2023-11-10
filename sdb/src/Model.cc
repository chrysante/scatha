#include "Model.h"

#include "Common.h"

using namespace sdb;

Model::Model(svm::VirtualMachine vm, uint8_t const* program):
    vm(std::move(vm)), disasm(disassemble(program)) {}

void Model::skipLine() { beep(); }

void Model::toggleExecution() { beep(); }

void Model::enterFunction() { beep(); }

void Model::exitFunction() { beep(); }
