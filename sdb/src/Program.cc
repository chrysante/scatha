#include "Program.h"

#include <iostream>

using namespace sdb;

void Program::skipLine() {
    ++_current;
    if (_current >= _insts.size()) {
        _current = 0;
    }
}

void Program::toggleExecution() { _running ^= true; }

void Program::enterFunction() { std::cout << '\007'; }

void Program::exitFunction() { std::cout << '\007'; }
