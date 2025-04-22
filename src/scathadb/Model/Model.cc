#include "Model/Model.h"

#include <condition_variable>
#include <fstream>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include <nlohmann/json.hpp>
#include <scatha/DebugInfo/DebugInfo.h>
#include <scdis/Disassembler.h>
#include <svm/Util.h>
#include <svm/VirtualMemory.h>
#include <utl/strcat.hpp>

#include "Model/Events.h"

using namespace sdb;

static decltype(auto) locked(auto& mutex, auto&& fn) {
    std::lock_guard lock(mutex);
    return fn();
}

Model::Model(std::shared_ptr<Messenger> messenger):
    _messenger(std::move(messenger)),
    executor(_messenger),
    breakpointManager(_messenger, disasm.indexMap(), sourceDbg),
    _stdout([this] { _messenger->send_now(PatientConsoleOutputEvent{}); }) {
    executor.writeVM().get().setIOStreams(nullptr, &_stdout);
}

static scatha::DebugInfoMap readDebugInfo(std::filesystem::path inputPath) {
    inputPath += ".scdsym";
    std::fstream file(inputPath, std::ios::in);
    if (!file) return {};
    auto j = nlohmann::json::parse(file);
    return scatha::DebugInfoMap::deserialize(j);
}

void Model::loadProgram(std::filesystem::path filepath) {
    executor.stopExecution();
    auto program = svm::readBinaryFromFile(filepath.string());
    if (program.empty()) {
        std::string progName = filepath.stem();
        auto msg =
            utl::strcat("Failed to load ", progName, ". Binary is empty.\n");
        throw std::runtime_error(msg);
    }
    auto vm = executor.writeVM();
    vm.get().setLibdir(filepath.parent_path());
    _currentFilepath = filepath;
    auto debugInfo = readDebugInfo(filepath);
    disasm = scdis::disassemble(program, debugInfo);
    sourceDbg = SourceDebugInfo::Make(debugInfo);
    executor.setBinary(program);
    breakpointManager.setProgramData(program);
    _messenger->send_buffered(ReloadUIRequest{});
}

void Model::unloadProgram() {
    executor.stopExecution();
    clearBreakpoints();
    _currentFilepath.clear();
    auto vm = executor.writeVM();
    vm.get().reset();
    disasm = {};
    sourceDbg = {};
    breakpointManager.clearAll();
}

bool Model::isProgramLoaded() const { return !_currentFilepath.empty(); }

void Model::setArguments(std::vector<std::string> arguments) {
    runArguments = std::move(arguments);
}

void Model::startExecution() {
    executor.stopExecution();
    _stdout.str({});
    executor.startExecution();
}

void Model::toggleInstBreakpoint(size_t instIndex) {
    breakpointManager.toggleInstBreakpoint(instIndex);
}

bool Model::toggleSourceBreakpoint(SourceLine line) {
    return breakpointManager.toggleSourceLineBreakpoint(line);
}

bool Model::hasInstBreakpoint(size_t instIndex) const {
    return breakpointManager.hasInstBreakpoint(instIndex);
}

bool Model::hasSourceBreakpoint(SourceLine line) const {
    return breakpointManager.hasSourceLineBreakpoint(line);
}

void Model::clearBreakpoints() { breakpointManager.clearAll(); }
