#include "REPL.h"

#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Print.h>
#include <scatha/Opt/PassManager.h>
#include <scatha/Opt/Pipeline.h>
#include <scatha/Opt/PipelineError.h>
#include <termfmt/termfmt.h>

#include "Parser.h"
#include "Util.h"

using namespace scatha;
using namespace passtool;

REPL::REPL(CLI::App* cli): Command(cli, "repl") {
    app()->add_option("files", paths);
}

int REPL::run() {
    if (paths.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }
    auto path = paths.front();
    auto [ctx, mod] = parseFile(path);
    header("Parsed program");
    ir::print(mod);

    while (true) {
        std::cout << tfmt::format(tfmt::Bold | tfmt::BrightGrey, ">>") << " ";
        std::string command;
        std::getline(std::cin, command);
        if (command == "q" || command == "quit") {
            return 0;
        }
        opt::Pipeline pipeline;
        try {
            pipeline = opt::PassManager::makePipeline(command);
            if (!pipeline) {
                std::cout << Error << "Invalid pipeline command" << std::endl;
                continue;
            }
        }
        catch (opt::PipelineError const& error) {
            std::cout << Error << error.what() << std::endl;
            continue;
        }
        bool modified = pipeline(ctx, mod);
        header("");
        ir::print(mod);
        std::cout << "Modified: " << std::boolalpha
                  << tfmt::format(tfmt::Bold, modified) << std::endl;
    }
}
