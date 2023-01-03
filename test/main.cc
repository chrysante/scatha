#define CATCH_CONFIG_MAIN
#include <Catch/Catch2.hpp>

#include "VM/VirtualMachine.h"

/// Stupid hack until I update to new catch version
static int const init = []{
    using namespace scatha;
    vm::VirtualMachine::setDefaultRegisterCount(1024);
    return 0;
}();
