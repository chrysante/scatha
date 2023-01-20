#define CATCH_CONFIG_MAIN
#include <Catch/Catch2.hpp>

#include <svm/VirtualMachine.h>

/// Stupid hack until I update to new catch version
static int const init = [] {
    svm::VirtualMachine::setDefaultRegisterCount(1024);
    return 0;
}();
