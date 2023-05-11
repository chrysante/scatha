#include "HostIntegration.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <scatha/Runtime/Compiler.h>
#include <scatha/Runtime/Function.h>
#include <scatha/Runtime/Program.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

using namespace playground;
using namespace scatha;

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

    auto cppCallbackID = compiler.declareFunction("cppCallback", Int, { Int });
    if (!cppCallbackID) {
        std::cout << "Failed to declare cppCallback\n";
    }

    compiler.addSource(text);

    auto prog = compiler.compile();
    if (!prog) {
        return;
    }

    svm::VirtualMachine vm;

    vm.loadBinary(prog->binary());

    int cppVar = 0;

    auto const callback = [&](long arg) {
        std::cout << "Hello from C++ land\n";
        std::cout << "cppVar = " << cppVar++ << std::endl;
        return arg * arg;
    };

    setExtFunction(&vm, *cppCallbackID, callback);

    auto mainAddress = prog->findAddress("main", { Int });
    if (!mainAddress) {
        std::cout << "Failed to find main\n";
        return;
    }
    int mainRetval = run<int>(&vm, *mainAddress);
    std::cout << "`main` returned: " << mainRetval << std::endl;

    auto alloc   = prog->findAddress("allocate", { Int });
    auto dealloc = prog->findAddress("deallocate", { { Byte, MutArrayRef } });

    auto data = run<std::span<char>>(&vm, alloc.value(), 40);

    std::strcpy(data.data(), "My message stored in foreign buffer\n");
    std::cout << data.data();

    run(&vm, dealloc.value(), data);
}
