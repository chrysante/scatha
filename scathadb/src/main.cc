#include <span>

#include <svm/ParseCLI.h>
#include <svm/Util.h>
#include <svm/VirtualMachine.h>
#include <utl/utility.hpp>

#include "Common.h"
#include "Debugger.h"
#include "Model.h"

using namespace sdb;

int main(int argc, char* argv[]) {
    Options options =
        parseArguments(std::span(argv + 1, utl::narrow_cast<size_t>(argc - 1)));
    Model model;
    if (options) {
        try {
            model.loadBinary(options);
        }
        catch (std::exception const& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
    Debugger debugger(&model);
    debugger.run();
}
