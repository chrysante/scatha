#include "OptTest.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <scatha/Runtime.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

using namespace playground;
using namespace scatha;

static auto const text = R"(
fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
}

fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar(10);
}

public fn main() -> int {
    print("Hello world\n");
    var i = 1;
    i = cppCallback(i);
    print(i);
    return 9;
}

public fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

public fn callback(n: int) {
    print("Callback\n");
    print(n);
    print(fac(n));
}
)";

void playground::optTest(std::filesystem::path path) {
    svm::VirtualMachine vm;

    Runtime rt(&vm);

    auto fn = [&](long value) {
        std::cout << "Recieved: " << value << std::endl;
        rt.run("callback", 6);
        return 7;
    };

    rt.addFunction({ "cppCallback", fn });

    rt.addSource(text);

    rt.compile();

    int retval = rt.run<int>("main");

    std::cout << "Program returned: " << retval << std::endl;

    int n = 8;
    std::cout << "fac(" << n << ") = " << rt.run<int>("fac", n) << std::endl;
}
