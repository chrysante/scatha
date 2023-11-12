#include <string>
#include <vector>

#include <range/v3/view.hpp>
#include <utl/utility.hpp>

#include "App/Debugger.h"
#include "Model/Model.h"
#include "Model/Options.h"

using namespace sdb;

static std::vector<std::string> makeArgVector(int argc, char* argv[]) {
    return std::span(argv, utl::narrow_cast<size_t>(argc)) |
           ranges::to<std::vector<std::string>>;
}

int main(int argc, char* argv[]) {
    auto args = makeArgVector(argc, argv);
    Options options = parseArguments(std::span(args).subspan(1));
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
