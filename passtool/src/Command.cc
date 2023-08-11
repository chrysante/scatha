#include "Command.h"

using namespace scatha;
using namespace passtool;

Command::Command(CLI::App* parent, std::string name):
    _app(parent->add_subcommand(name)) {}
