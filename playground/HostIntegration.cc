#include "HostIntegration.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <scatha/Runtime.h>

using namespace playground;
using namespace scatha;

static long cppCallback(long arg) {
    std::cout << "Hello from C++ land\n";
    return arg * arg;
}

void playground::hostIntegration(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        std::cout << "Failed to open " << path << std::endl;
        return;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    auto text = std::move(sstr).str();

    Compiler compiler;

    using enum BaseType;
    using enum Qualifier;

    auto cbID1 = compiler.declareFunction("cppCallback", Int, { Int });
    if (!cbID1) {
        std::cout << "Failed to declare cppCallback\n";
        return;
    }
    auto cbID2 = compiler.declareFunction("cppCallback2", Int, { Int });
    if (!cbID2) {
        std::cout << "Failed to declare cppCallback2\n";
        return;
    }

    compiler.addSource(text);

    auto prog = compiler.compile();
    if (!prog) {
        return;
    }

    svm::VirtualMachine vm;
    setExtFunction(&vm, *cbID1, [](long arg) { return cppCallback(arg); });

    vm.loadBinary(prog->binary());

    int cppVar = 0;

    auto const callback2 = [&](long arg) {
        std::cout << "Hello from C++ land again\n";
        std::cout << "cppVar = " << cppVar++ << std::endl;
        return arg * arg;
    };

    setExtFunction(&vm, *cbID2, callback2);

    auto mainAddress = prog->findAddress("main", { Int });
    if (!mainAddress) {
        std::cout << "Failed to find main\n";
        return;
    }
    int mainRetval = run<int>(&vm, *mainAddress);
    std::cout << "`main` returned: " << mainRetval << std::endl;

    auto alloc   = prog->findAddress("allocate", { Int });
    auto dealloc = prog->findAddress("deallocate", { { Byte, MutArrayRef } });
    auto print   = prog->findAddress("X.print", { { Byte, ArrayRef } });

    char const message[] = "My message stored in foreign buffer\n";

    auto data = run<std::span<char>>(&vm, alloc.value(), sizeof message);

    std::strcpy(data.data(), message);

    run(&vm, print.value(), data);

    run(&vm, dealloc.value(), data);
}
