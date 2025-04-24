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
#include "Util/Messenger.h"

using namespace sdb;

static decltype(auto) locked(auto& mutex, auto&& fn) {
    std::lock_guard lock(mutex);
    return fn();
}

Model::Model(std::shared_ptr<Messenger> messenger):
    _messenger(std::move(messenger)),
    executor(_messenger, disasm.indexMap(), sourceDbg),
    breakpointManager(_messenger, executor, disasm.indexMap(), sourceDbg),
    _stdout(std::make_unique<CallbackStringStream>(
        [this] { _messenger->send_now(PatientConsoleOutputEvent{}); })) {
    executor.writeVM().get().setIOStreams(nullptr, _stdout.get());
}

static scatha::DebugInfoMap readDebugInfo(std::filesystem::path inputPath) {
    inputPath += ".scdsym";
    std::fstream file(inputPath, std::ios::in);
    if (!file) return {};
    auto j = nlohmann::json::parse(file);
    return scatha::DebugInfoMap::deserialize(j);
}

void Model::loadProgram(std::filesystem::path filepath) {
    auto binary = svm::readBinaryFromFile(filepath.string());
    if (binary.empty()) {
        std::string progName = filepath.stem();
        auto msg =
            utl::strcat("Failed to load ", progName, ". Binary is empty.\n");
        throw std::runtime_error(msg);
    }
    loadProgram(binary, filepath.parent_path(), readDebugInfo(filepath));
    _currentFilepath = filepath;
}

void Model::loadProgram(
    std::span<uint8_t const> binary, std::filesystem::path runtimeLibDir,
    scatha::DebugInfoMap const& debugInfo,
    utl::function_view<SourceFile(std::filesystem::path)> sourceFileLoader) {
    _currentFilepath.clear();
    executor.stopExecution();
    breakpointManager.clearAll();
    auto vm = executor.writeVM();
    vm.get().setLibdir(runtimeLibDir);
    disasm = scdis::disassemble(binary, debugInfo);
    sourceDbg = SourceDebugInfo::Make(debugInfo, sourceFileLoader);
    executor.loadProgram(std::vector(binary.begin(), binary.end()));
    _messenger->send_buffered(ReloadUIRequest{});
    _isProgramLoaded = true;
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
    _isProgramLoaded = false;
}

bool Model::isProgramLoaded() const { return !_currentFilepath.empty(); }

void Model::setArguments(std::vector<std::string> arguments) {
    runArguments = std::move(arguments);
}

void Model::startExecution() {
    executor.stopExecution();
    _stdout->str({});
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
