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

using namespace sdb;

static decltype(auto) locked(auto& mutex, auto&& fn) {
    std::lock_guard lock(mutex);
    return fn();
}

Model::Model(UIHandle* uiHandle):
    uiHandle(uiHandle),
    executor(uiHandle),
    _stdout([uiHandle] { uiHandle->refresh(); }) {
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
    clearBreakpoints();
    auto binary = svm::readBinaryFromFile(filepath.string());
    if (binary.empty()) {
        std::string progName = filepath.stem();
        auto msg =
            utl::strcat("Failed to load ", progName, ". Binary is empty.\n");
        throw std::runtime_error(msg);
    }
    auto vm = executor.writeVM();
    vm.get().setLibdir(filepath.parent_path());
    _currentFilepath = filepath;
    auto debugInfo = readDebugInfo(filepath);
    disasm = scdis::disassemble(binary, debugInfo);
    sourceDbg = SourceDebugInfo::Make(debugInfo);
    executor.setBinary(std::move(binary));
    if (uiHandle) uiHandle->reload();
}

void Model::unloadProgram() {
    executor.stopExecution();
    clearBreakpoints();
    _currentFilepath.clear();
    auto vm = executor.writeVM();
    vm.get().reset();
    disasm = {};
    sourceDbg = {};
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

void Model::stepSourceLine() {}

void Model::toggleInstBreakpoint(size_t instIndex) {
    auto ipo = disasm.indexMap().indexToIpo(instIndex);
    instBreakpoints.toggle(ipo.value);
}

bool Model::toggleSourceBreakpoint(SourceLine line) {
    auto ipos = sourceDebug().sourceMap().toIpos(line);
    if (ipos.empty()) return false;
    sourceBreakpoints.toggle(ipos.front().value);
    return true;
}

bool Model::hasInstBreakpoint(size_t instIndex) const {
    auto ipo = disasm.indexMap().indexToIpo(instIndex);
    return instBreakpoints.at(ipo.value);
}

bool Model::hasSourceBreakpoint(SourceLine line) const {
    auto offsets = sourceDebug().sourceMap().toIpos(line);
    if (offsets.empty()) return false;
    return sourceBreakpoints.at(offsets.front().value);
}

void Model::clearBreakpoints() {
    instBreakpoints.clear();
    sourceBreakpoints.clear();
}
