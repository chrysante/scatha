#include "Model/BreakpointManager.h"

#include <atomic>

#include <scbinutil/OpCode.h>
#include <scbinutil/ProgramView.h>
#include <svm/VirtualMachine.h>

#include "Model/Events.h"

using namespace sdb;

void BreakpointPatcher::addBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.insert(ipo);
    removalQueue.erase(ipo);
    activeBreakpoints.insert(ipo);
}

void BreakpointPatcher::removeBreakpoint(scdis::InstructionPointerOffset ipo) {
    insertQueue.erase(ipo);
    removalQueue.insert(ipo);
    activeBreakpoints.erase(ipo);
}

void BreakpointPatcher::pushBreakpoint(scdis::InstructionPointerOffset ipo) {
    bool haveBreakpoint = activeBreakpoints.contains(ipo);
    stackMap[ipo].push(haveBreakpoint);
    if (haveBreakpoint) removeBreakpoint(ipo);
}

void BreakpointPatcher::popBreakpoint(scdis::InstructionPointerOffset ipo) {
    auto itr = stackMap.find(ipo);
    assert(itr != stackMap.end() && "Have no breakpoint pushed");
    auto& stack = itr->second;
    bool haveBreakpoint = stack.pop();
    if (stack.empty()) stackMap.erase(itr);
    if (haveBreakpoint) addBreakpoint(ipo);
}

void BreakpointPatcher::removeAll() {
    insertQueue.clear();
    removalQueue.insert(activeBreakpoints.begin(), activeBreakpoints.end());
    activeBreakpoints.clear();
}

void BreakpointPatcher::patchInstructionStream(uint8_t* bin) {
    auto store = [=](scdis::InstructionPointerOffset ipo, uint8_t value) {
        auto ref = std::atomic_ref<uint8_t>(bin[ipo.value]);
        ref.store(value);
    };
    for (auto ipo: insertQueue)
        store(ipo, (uint8_t)scbinutil::OpCode::trap);
    for (auto ipo: removalQueue)
        store(ipo, binary[ipo.value]);
    insertQueue.clear();
    removalQueue.clear();
}

void BreakpointPatcher::setProgramData(std::span<uint8_t const> progData) {
    scbinutil::ProgramView program(progData.data());
    binary.assign(program.binary.begin(), program.binary.end());
}

BreakpointManager::BreakpointManager(std::shared_ptr<utl::messenger> messenger,
                                     scdis::IpoIndexMap const& ipoIndexMap,
                                     SourceLocationMap const& sourceLocMap):
    transceiver(std::move(messenger)),
    ipoIndexMap(ipoIndexMap),
    sourceLocMap(sourceLocMap) {
    listen([this](WillBeginExecution event) {
        for (auto instIndex: instBreakpointSet) {
            auto ipo = this->ipoIndexMap.indexToIpo(instIndex);
            patcher.addBreakpoint(ipo);
        }
        for (auto sourceLine: sourceLineBreakpointSet) {
            auto ipos = this->sourceLocMap.toIpos(sourceLine);
            assert(!ipos.empty() &&
                   "Shouldn't have added this to the breakpoint set");
            patcher.addBreakpoint(ipos.front());
        }
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](WillStepInstruction event) {
        patcher.pushBreakpoint(event.ipo);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
    listen([this](DidStepInstruction event) {
        patcher.popBreakpoint(event.ipo);
        patcher.patchInstructionStream(event.vm.getBinaryPointer());
    });
}

static bool isExecIdle(utl::emitter<utl::messenger> const& emitter) {
    bool isIdle = true;
    emitter.send_now(IsExecIdle{ &isIdle });
    return isIdle;
}

void BreakpointManager::toggleInstBreakpoint(size_t instIndex) {
    auto [itr, success] = instBreakpointSet.insert(instIndex);
    if (!success) instBreakpointSet.erase(itr);
    if (isExecIdle(*this)) return;
    auto ipo = ipoIndexMap.indexToIpo(instIndex);
    if (success)
        patcher.addBreakpoint(ipo);
    else
        patcher.removeBreakpoint(ipo);
    install();
}

bool BreakpointManager::hasInstBreakpoint(size_t instIndex) const {
    return instBreakpointSet.contains(instIndex);
}

bool BreakpointManager::toggleSourceLineBreakpoint(SourceLine line) {
    auto ipos = sourceLocMap.toIpos(line);
    if (ipos.empty()) return false;
    auto [itr, success] = sourceLineBreakpointSet.insert(line);
    if (!success) sourceLineBreakpointSet.erase(itr);
    if (isExecIdle(*this)) return true;
    if (success)
        patcher.addBreakpoint(ipos.front());
    else
        patcher.removeBreakpoint(ipos.front());
    install();
    return true;
}

bool BreakpointManager::hasSourceLineBreakpoint(SourceLine line) const {
    return sourceLineBreakpointSet.contains(line);
}

void BreakpointManager::clearAll() {
    instBreakpointSet.clear();
    if (isExecIdle(*this)) return;
    patcher.removeAll();
    install();
}

void BreakpointManager::setProgramData(std::span<uint8_t const> progData) {
    clearAll();
    patcher.setProgramData(progData);
}

void BreakpointManager::install() {
    send_now(DoInterruptedOnVM{ [this](svm::VirtualMachine& vm) {
        patcher.patchInstructionStream(vm.getBinaryPointer());
    } });
}
