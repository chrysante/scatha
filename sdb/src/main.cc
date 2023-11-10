#include "Debugger.h"
#include "Model.h"

#include <svm/ParseCLI.h>
#include <svm/Util.h>
#include <svm/VirtualMachine.h>

using namespace sdb;

int main(int argc, char* argv[]) {
    svm::Options options = svm::parseCLI(argc, argv);
    std::string progName = options.filepath.stem();

    svm::VirtualMachine vm;
    std::vector<uint8_t> binary;
    try {
        binary = svm::readBinaryFromFile(options.filepath.string());
        if (binary.empty()) {
            std::cerr << "Failed to run " << progName << ". Binary is empty.\n";
            return -1;
        }
        vm.loadBinary(binary.data());
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    auto execArg = setupArguments(vm, options.arguments);

    Model model(std::move(vm), binary.data());
    Debugger debugger(&model);
    debugger.run();
}
