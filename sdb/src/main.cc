#include "Debugger.h"
#include "Program.h"

using namespace sdb;

int main() {
    Program program({
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
        { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },  { "mov %0, %1" },
        { "add %0, %1" }, { "terminate" },  { "mov %0, %1" }, { "add %0, %1" },
        { "terminate" },  { "mov %0, %1" }, { "add %0, %1" }, { "terminate" },
    });
    Debugger debugger(&program);
    debugger.run();
}
